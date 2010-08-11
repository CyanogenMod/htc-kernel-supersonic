/*
 *	kineto_gan.c
 *
 * Linux loadable kernel module to implement a virtual ethernet interfacesupport the Kineto GAN client.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>	       /* struct iphdr */
#include <linux/tcp.h>	       /* struct tcphdr */

#include <linux/skbuff.h>
#include <linux/wakelock.h>
#include <linux/net.h>
#include    <linux/socket.h>


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
/* For socket etc */
#include <linux/net.h>
#include <net/sock.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/socket.h>
#include <linux/smp_lock.h>
#include <linux/slab.h>
#include <linux/kthread.h>

#include <linux/if_arp.h>

#define MODULE_NAME "gannet"

#define KINETO_VIF_LINK_PORT	13010
#define KINETO_PS_PORT_2	13001

struct gannet_private {
    struct net_device_stats stats;
    struct sockaddr_in tx_addr;
    struct sockaddr_in rx_addr;
    struct socket *tx_sock;
    struct socket *rx_sock;
};

struct gannet_thread {
    struct task_struct *thread;
    struct net_device *dev;
    struct gannet_private *priv;
};
static struct gannet_thread *gthread;
static struct net_device *gdev;
static int gthreadquit;

static int count_this_packet(void *_hdr, int len)
{
    struct ethhdr *hdr = _hdr;

    if (len >= ETH_HLEN && hdr->h_proto == htons(ETH_P_ARP))
	return 0;

    return 1;
}

static int gannet_open(struct net_device *dev)
{
    printk(KERN_DEBUG "gannet_open()\n");
    netif_start_queue(dev);
    return 0;
}

static int gannet_stop(struct net_device *dev)
{
    printk(KERN_DEBUG "gannet_stop()\n");
    netif_stop_queue(dev);
    return 0;
}


static int ksocket_sendto(struct socket *sock,
	      struct sockaddr_in *addr,
	      unsigned char *buf,
	      int len)
{
    struct msghdr msg;
    struct iovec iov;
    mm_segment_t oldfs;
    int size = 0;

    if (sock->sk == NULL)
	return 0;

    iov.iov_base = buf;
    iov.iov_len = len;

    msg.msg_flags = 0;
    msg.msg_name = addr;
    msg.msg_namelen  = sizeof(struct sockaddr_in);
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    size = sock_sendmsg(sock, &msg, len);
    set_fs(oldfs);

    return size;
}

static int gannet_xmit(struct sk_buff *skb, struct net_device *dev)
{
    /* write skb->data of skb->len to udp socket */
    struct gannet_private *p = netdev_priv(dev);
    char *data;
    int len;


    /*
    if(skb->protocol != ETH_P_IP)
    {
	pr_info("dropping packet, not IP");
	dev_kfree_skb_irq(skb);
	return NET_XMIT_DROP;
    }
    */
    if (NULL == ipip_hdr(skb)) {
	printk(KERN_WARNING "dropping packet, no IP header\n");
	dev_kfree_skb_irq(skb);
	return NET_XMIT_DROP;
    }

    data = skb->data;
    len = skb->len;

    if (len < (sizeof(struct ethhdr) + sizeof(struct iphdr))) {
	printk(KERN_WARNING "gannet: Packet too short (%i bytes)\n",
				    len);
	     return 0;
    }

    /* Remove ethernet header */
    data += 14;
    len -= 14;

    if (len != ksocket_sendto(p->tx_sock, &p->tx_addr, data, len)) {
	printk(KERN_ERR "gannet sendto() failed, dropping packet\n");
    } else {
	if (count_this_packet(data, len)) {
		p->stats.tx_packets++;
		p->stats.tx_bytes += len;
	}
    }

    dev_kfree_skb_irq(skb);
    return 0;
}

static struct net_device_stats *gannet_get_stats(struct net_device *dev)
{
    struct gannet_private *p = netdev_priv(dev);
    return &p->stats;
}

static void gannet_set_multicast_list(struct net_device *dev)
{
}

static void gannet_tx_timeout(struct net_device *dev)
{
    printk(KERN_DEBUG "gannet_tx_timeout()\n");
}

static int ksocket_receive(struct socket *sock,
	       struct sockaddr_in *addr,
	       unsigned char *buf,
	       int len)
{
	struct msghdr msg;
	struct iovec iov;
	mm_segment_t oldfs;
	int size = 0;

    if (sock->sk == NULL)
	return 0;

	iov.iov_base = buf;
	iov.iov_len = len;

	msg.msg_flags = 0;
	msg.msg_name = addr;
	msg.msg_namelen	 = sizeof(struct sockaddr_in);
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	size = sock_recvmsg(sock, &msg, len, msg.msg_flags);
	set_fs(oldfs);

	return size;
}



static void rx(unsigned char *buf, int len)
{
    struct sk_buff *skb;
    void *ptr = 0;
    int sz;
    int r;

    sz = len;

    if (sz > 1514) {
	printk(KERN_ERR MODULE_NAME " discarding %d len\n", sz);
	ptr = 0;
    } else {
	skb = dev_alloc_skb(sz + 14 + NET_IP_ALIGN);
	if (skb == NULL) {
		printk(KERN_ERR MODULE_NAME " cannot allocate skb\n");
	} else {
		skb_reserve(skb, NET_IP_ALIGN);

		ptr = skb_put(skb, 14); /* ethernet hdr */
		memcpy(&((unsigned char *)ptr)[6],
			gthread->dev->dev_addr,
			6);

		ptr = skb_put(skb, sz);
		memcpy(ptr, buf, sz);

		skb->dev = gthread->dev;
		skb->protocol = eth_type_trans(skb, gthread->dev);
		skb->protocol = htons(ETH_P_IP);
		skb->ip_summed = CHECKSUM_NONE; /* check it */

		skb->pkt_type = PACKET_HOST;

		if (count_this_packet(ptr, skb->len)) {
			gthread->priv->stats.rx_packets++;
			gthread->priv->stats.rx_bytes += skb->len;
		}
		r = netif_rx(skb);
	}
    }
}


static void gannet_recvloop(void)
{
    int size;
    int bufsize = 1600;
    unsigned char buf[bufsize+1];

    /* kernel thread initialization */
    lock_kernel();

    current->flags |= PF_NOFREEZE;

    /* daemonize (take care with signals, after daemonize they are disabled) */
    daemonize(MODULE_NAME);
    allow_signal(SIGKILL);
    unlock_kernel();


    /* main loop */
    while (!gthreadquit) {
	memset(&buf, 0, bufsize+1);
	size = ksocket_receive(gthread->priv->rx_sock,
		       &gthread->priv->rx_addr,
		       buf,
		       bufsize);

	if (signal_pending(current))
		break;

	if (size < 0) {
		printk(KERN_ERR MODULE_NAME": error getting datagram, "
		       "sock_recvmsg error = %d\n", size);
	} else {
		/* send to kernel */
		rx(buf, size);
	}
    }

    printk(KERN_INFO "gannet thread exit\n");
}



static const struct net_device_ops gannet_netdev_ops = {
    .ndo_open = gannet_open,
    .ndo_stop = gannet_stop,
    .ndo_start_xmit = gannet_xmit,
    .ndo_get_stats = gannet_get_stats,
    .ndo_set_multicast_list = gannet_set_multicast_list,
    .ndo_tx_timeout = gannet_tx_timeout,
    .ndo_change_mtu = NULL,
};

static void __init gannet_setup(struct net_device *dev)
{
    printk(KERN_INFO "gannet_setup\n");

    ether_setup(dev);

    dev->mtu = 1320;
/*
    dev->open = gannet_open;
    dev->stop = gannet_stop;
    dev->hard_start_xmit = gannet_xmit;
    dev->get_stats = gannet_get_stats;
    dev->set_multicast_list = gannet_set_multicast_list;
    dev->tx_timeout = gannet_tx_timeout;
 */
    dev->netdev_ops = &gannet_netdev_ops;
    /* The minimum time (in jiffies) that should pass before the networking
     * layer decides that a transmission timeout has occurred and calls the
     * driver's tx_time-out function.
     */
    dev->watchdog_timeo = 200;

    /* keep the default flags, just add NOARP */
    dev->flags		 |= IFF_NOARP;

    random_ether_addr(dev->dev_addr);

    netif_start_queue(dev);
}

static int __init gannet_init(void)
{
    int ret;
    struct net_device *dev;
    struct gannet_private *p;

    dev = alloc_netdev(sizeof(struct gannet_private),
		"gannet%d",
		gannet_setup);
    if (NULL == dev)
	return -ENOMEM;

    ret = register_netdev(dev);
    if (ret) {
	printk(KERN_ERR "gannet failed to register netdev\n");
	free_netdev(dev);
	return ret;
    }

    gdev = dev;
    p = netdev_priv(dev);

    /* Create tx socket */
    ret = sock_create(PF_INET, SOCK_DGRAM, IPPROTO_UDP, &p->tx_sock);
    if (ret < 0) {
	printk(KERN_ERR "gannet tx socket() failed, failing init.\n");
	unregister_netdev(dev);
	free_netdev(dev);
	return -EIO; /* I/O error */
    }
    memset(&p->tx_addr, 0, sizeof(p->tx_addr));
    p->tx_addr.sin_family = AF_INET;
    p->tx_addr.sin_port = htons(KINETO_PS_PORT_2);
    p->tx_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    /* Create rx socket */
    ret = sock_create(PF_INET, SOCK_DGRAM, IPPROTO_UDP, &p->rx_sock);
    if (ret < 0) {
	printk(KERN_ERR "gannet rx socket() failed, failing init.\n");
	sock_release(p->tx_sock);
	unregister_netdev(dev);
	free_netdev(dev);
	return -EIO; /* I/O error? There's nothing more applicable. */
    }
    memset(&p->rx_addr, 0, sizeof(p->rx_addr));
    p->rx_addr.sin_family = AF_INET;
    p->rx_addr.sin_port = htons(KINETO_VIF_LINK_PORT);
    p->rx_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* INADDR_LOOPBACK
							for security? */

    /* Bind rx socket */
    ret = p->rx_sock->ops->bind(p->rx_sock,
		    (struct sockaddr *)&p->rx_addr,
		    sizeof(struct sockaddr));
    if (ret < 0) {
	printk(KERN_ERR "gannet rx socket() bind failed.\n");
	sock_release(p->tx_sock);
	sock_release(p->rx_sock);
	unregister_netdev(dev);
	free_netdev(dev);
	return -EIO; /* I/O error */
    }


    /* Create kernel thread for rx loop */
    gthread = kmalloc(sizeof(struct gannet_thread), GFP_KERNEL);
    memset(gthread, 0, sizeof(struct gannet_thread));

    /* Store ref to private info */
    gthread->dev = dev;
    gthread->priv = p;

    printk(KERN_INFO "gannet starting kernel thread\n");
    gthread->thread = kthread_run((void *)gannet_recvloop,
		      NULL,
		      MODULE_NAME);
    if (IS_ERR(gthread->thread)) {
	printk(KERN_ERR MODULE_NAME": unable to start kernel thread\n");
	    sock_release(p->tx_sock);
	    sock_release(p->rx_sock);
	    free_netdev(dev);
	    kfree(gthread);
	    gthread = NULL;
	    return -ENOMEM;
    }
    printk(KERN_INFO "gannet initialized OK\n");

    return 0;
}

static void __exit gannet_exit(void)
{
    gthreadquit = 1;
    if (NULL != gdev) {
	struct gannet_private *p = netdev_priv(gdev);

	sock_release(p->rx_sock);
	sock_release(p->tx_sock);
	unregister_netdev(gdev);
	free_netdev(gdev);
    }
}

module_init(gannet_init);
module_exit(gannet_exit);

MODULE_DESCRIPTION("Kineto GAN Virtual Ethernet Device");
MODULE_ALIAS("gannet");
MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Jon Read <jread@kineto.com>");
MODULE_VERSION("1.0");
