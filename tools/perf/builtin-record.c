/*
 * builtin-record.c
 *
 * Builtin record command: Record the profile of a workload
 * (or a CPU, or a PID) into the perf.data output file - for
 * later analysis via perf report.
 */
#include "builtin.h"

#include "perf.h"

#include "util/util.h"
#include "util/parse-options.h"
#include "util/parse-events.h"
#include "util/string.h"

#include "util/header.h"
#include "util/event.h"
#include "util/debug.h"
#include "util/trace-event.h"

#include <unistd.h>
#include <sched.h>

#define ALIGN(x, a)		__ALIGN_MASK(x, (typeof(x))(a)-1)
#define __ALIGN_MASK(x, mask)	(((x)+(mask))&~(mask))

static int			fd[MAX_NR_CPUS][MAX_COUNTERS];

static long			default_interval		= 100000;

static int			nr_cpus				= 0;
static unsigned int		page_size;
static unsigned int		mmap_pages			= 128;
static int			freq				= 0;
static int			output;
static const char		*output_name			= "perf.data";
static int			group				= 0;
static unsigned int		realtime_prio			= 0;
static int			raw_samples			= 0;
static int			system_wide			= 0;
static int			profile_cpu			= -1;
static pid_t			target_pid			= -1;
static pid_t			child_pid			= -1;
static int			inherit				= 1;
static int			force				= 0;
static int			append_file			= 0;
static int			call_graph			= 0;
static int			inherit_stat			= 0;
static int			no_samples			= 0;
static int			sample_address			= 0;
static int			multiplex			= 0;
static int			multiplex_fd			= -1;

static long			samples;
static struct timeval		last_read;
static struct timeval		this_read;

static u64			bytes_written;

static struct pollfd		event_array[MAX_NR_CPUS * MAX_COUNTERS];

static int			nr_poll;
static int			nr_cpu;

static int			file_new = 1;

struct perf_header		*header;

struct mmap_data {
	int			counter;
	void			*base;
	unsigned int		mask;
	unsigned int		prev;
};

static struct mmap_data		mmap_array[MAX_NR_CPUS][MAX_COUNTERS];

static unsigned long mmap_read_head(struct mmap_data *md)
{
	struct perf_event_mmap_page *pc = md->base;
	long head;

	head = pc->data_head;
	rmb();

	return head;
}

static void mmap_write_tail(struct mmap_data *md, unsigned long tail)
{
	struct perf_event_mmap_page *pc = md->base;

	/*
	 * ensure all reads are done before we write the tail out.
	 */
	/* mb(); */
	pc->data_tail = tail;
}

static void write_output(void *buf, size_t size)
{
	while (size) {
		int ret = write(output, buf, size);

		if (ret < 0)
			die("failed to write");

		size -= ret;
		buf += ret;

		bytes_written += ret;
	}
}

static void mmap_read(struct mmap_data *md)
{
	unsigned int head = mmap_read_head(md);
	unsigned int old = md->prev;
	unsigned char *data = md->base + page_size;
	unsigned long size;
	void *buf;
	int diff;

	gettimeofday(&this_read, NULL);

	/*
	 * If we're further behind than half the buffer, there's a chance
	 * the writer will bite our tail and mess up the samples under us.
	 *
	 * If we somehow ended up ahead of the head, we got messed up.
	 *
	 * In either case, truncate and restart at head.
	 */
	diff = head - old;
	if (diff < 0) {
		struct timeval iv;
		unsigned long msecs;

		timersub(&this_read, &last_read, &iv);
		msecs = iv.tv_sec*1000 + iv.tv_usec/1000;

		fprintf(stderr, "WARNING: failed to keep up with mmap data."
				"  Last read %lu msecs ago.\n", msecs);

		/*
		 * head points to a known good entry, start there.
		 */
		old = head;
	}

	last_read = this_read;

	if (old != head)
		samples++;

	size = head - old;

	if ((old & md->mask) + size != (head & md->mask)) {
		buf = &data[old & md->mask];
		size = md->mask + 1 - (old & md->mask);
		old += size;

		write_output(buf, size);
	}

	buf = &data[old & md->mask];
	size = head - old;
	old += size;

	write_output(buf, size);

	md->prev = old;
	mmap_write_tail(md, old);
}

static volatile int done = 0;
static volatile int signr = -1;

static void sig_handler(int sig)
{
	done = 1;
	signr = sig;
}

static void sig_atexit(void)
{
	if (child_pid != -1)
		kill(child_pid, SIGTERM);

	if (signr == -1)
		return;

	signal(signr, SIG_DFL);
	kill(getpid(), signr);
}

static pid_t pid_synthesize_comm_event(pid_t pid, int full)
{
	struct comm_event comm_ev;
	char filename[PATH_MAX];
	char bf[BUFSIZ];
	FILE *fp;
	size_t size = 0;
	DIR *tasks;
	struct dirent dirent, *next;
	pid_t tgid = 0;

	snprintf(filename, sizeof(filename), "/proc/%d/status", pid);

	fp = fopen(filename, "r");
	if (fp == NULL) {
		/*
		 * We raced with a task exiting - just return:
		 */
		if (verbose)
			fprintf(stderr, "couldn't open %s\n", filename);
		return 0;
	}

	memset(&comm_ev, 0, sizeof(comm_ev));
	while (!comm_ev.comm[0] || !comm_ev.pid) {
		if (fgets(bf, sizeof(bf), fp) == NULL)
			goto out_failure;

		if (memcmp(bf, "Name:", 5) == 0) {
			char *name = bf + 5;
			while (*name && isspace(*name))
				++name;
			size = strlen(name) - 1;
			memcpy(comm_ev.comm, name, size++);
		} else if (memcmp(bf, "Tgid:", 5) == 0) {
			char *tgids = bf + 5;
			while (*tgids && isspace(*tgids))
				++tgids;
			tgid = comm_ev.pid = atoi(tgids);
		}
	}

	comm_ev.header.type = PERF_RECORD_COMM;
	size = ALIGN(size, sizeof(u64));
	comm_ev.header.size = sizeof(comm_ev) - (sizeof(comm_ev.comm) - size);

	if (!full) {
		comm_ev.tid = pid;

		write_output(&comm_ev, comm_ev.header.size);
		goto out_fclose;
	}

	snprintf(filename, sizeof(filename), "/proc/%d/task", pid);

	tasks = opendir(filename);
	while (!readdir_r(tasks, &dirent, &next) && next) {
		char *end;
		pid = strtol(dirent.d_name, &end, 10);
		if (*end)
			continue;

		comm_ev.tid = pid;

		write_output(&comm_ev, comm_ev.header.size);
	}
	closedir(tasks);

out_fclose:
	fclose(fp);
	return tgid;

out_failure:
	fprintf(stderr, "couldn't get COMM and pgid, malformed %s\n",
		filename);
	exit(EXIT_FAILURE);
}

static void pid_synthesize_mmap_samples(pid_t pid, pid_t tgid)
{
	char filename[PATH_MAX];
	FILE *fp;

	snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);

	fp = fopen(filename, "r");
	if (fp == NULL) {
		/*
		 * We raced with a task exiting - just return:
		 */
		if (verbose)
			fprintf(stderr, "couldn't open %s\n", filename);
		return;
	}
	while (1) {
		char bf[BUFSIZ], *pbf = bf;
		struct mmap_event mmap_ev = {
			.header = { .type = PERF_RECORD_MMAP },
		};
		int n;
		size_t size;
		if (fgets(bf, sizeof(bf), fp) == NULL)
			break;

		/* 00400000-0040c000 r-xp 00000000 fd:01 41038  /bin/cat */
		n = hex2u64(pbf, &mmap_ev.start);
		if (n < 0)
			continue;
		pbf += n + 1;
		n = hex2u64(pbf, &mmap_ev.len);
		if (n < 0)
			continue;
		pbf += n + 3;
		if (*pbf == 'x') { /* vm_exec */
			char *execname = strchr(bf, '/');

			/* Catch VDSO */
			if (execname == NULL)
				execname = strstr(bf, "[vdso]");

			if (execname == NULL)
				continue;

			size = strlen(execname);
			execname[size - 1] = '\0'; /* Remove \n */
			memcpy(mmap_ev.filename, execname, size);
			size = ALIGN(size, sizeof(u64));
			mmap_ev.len -= mmap_ev.start;
			mmap_ev.header.size = (sizeof(mmap_ev) -
					       (sizeof(mmap_ev.filename) - size));
			mmap_ev.pid = tgid;
			mmap_ev.tid = pid;

			write_output(&mmap_ev, mmap_ev.header.size);
		}
	}

	fclose(fp);
}

static void synthesize_all(void)
{
	DIR *proc;
	struct dirent dirent, *next;

	proc = opendir("/proc");

	while (!readdir_r(proc, &dirent, &next) && next) {
		char *end;
		pid_t pid, tgid;

		pid = strtol(dirent.d_name, &end, 10);
		if (*end) /* only interested in proper numerical dirents */
			continue;

		tgid = pid_synthesize_comm_event(pid, 1);
		pid_synthesize_mmap_samples(pid, tgid);
	}

	closedir(proc);
}

static int group_fd;

static struct perf_header_attr *get_header_attr(struct perf_event_attr *a, int nr)
{
	struct perf_header_attr *h_attr;

	if (nr < header->attrs) {
		h_attr = header->attr[nr];
	} else {
		h_attr = perf_header_attr__new(a);
		perf_header__add_attr(header, h_attr);
	}

	return h_attr;
}

static void create_counter(int counter, int cpu, pid_t pid)
{
	struct perf_event_attr *attr = attrs + counter;
	struct perf_header_attr *h_attr;
	int track = !counter; /* only the first counter needs these */
	struct {
		u64 count;
		u64 time_enabled;
		u64 time_running;
		u64 id;
	} read_data;

	attr->read_format	= PERF_FORMAT_TOTAL_TIME_ENABLED |
				  PERF_FORMAT_TOTAL_TIME_RUNNING |
				  PERF_FORMAT_ID;

	attr->sample_type	|= PERF_SAMPLE_IP | PERF_SAMPLE_TID;

	if (freq) {
		attr->sample_type	|= PERF_SAMPLE_PERIOD;
		attr->freq		= 1;
		attr->sample_freq	= freq;
	}

	if (no_samples)
		attr->sample_freq = 0;

	if (inherit_stat)
		attr->inherit_stat = 1;

	if (sample_address)
		attr->sample_type	|= PERF_SAMPLE_ADDR;

	if (call_graph)
		attr->sample_type	|= PERF_SAMPLE_CALLCHAIN;

	if (raw_samples) {
		attr->sample_type	|= PERF_SAMPLE_TIME;
		attr->sample_type	|= PERF_SAMPLE_RAW;
		attr->sample_type	|= PERF_SAMPLE_CPU;
	}

	attr->mmap		= track;
	attr->comm		= track;
	attr->inherit		= (cpu < 0) && inherit;
	attr->disabled		= 1;

try_again:
	fd[nr_cpu][counter] = sys_perf_event_open(attr, pid, cpu, group_fd, 0);

	if (fd[nr_cpu][counter] < 0) {
		int err = errno;

		if (err == EPERM || err == EACCES)
			die("Permission error - are you root?\n");
		else if (err ==  ENODEV && profile_cpu != -1)
			die("No such device - did you specify an out-of-range profile CPU?\n");

		/*
		 * If it's cycles then fall back to hrtimer
		 * based cpu-clock-tick sw counter, which
		 * is always available even if no PMU support:
		 */
		if (attr->type == PERF_TYPE_HARDWARE
			&& attr->config == PERF_COUNT_HW_CPU_CYCLES) {

			if (verbose)
				warning(" ... trying to fall back to cpu-clock-ticks\n");
			attr->type = PERF_TYPE_SOFTWARE;
			attr->config = PERF_COUNT_SW_CPU_CLOCK;
			goto try_again;
		}
		printf("\n");
		error("perfcounter syscall returned with %d (%s)\n",
			fd[nr_cpu][counter], strerror(err));
		die("No CONFIG_PERF_EVENTS=y kernel support configured?\n");
		exit(-1);
	}

	h_attr = get_header_attr(attr, counter);

	if (!file_new) {
		if (memcmp(&h_attr->attr, attr, sizeof(*attr))) {
			fprintf(stderr, "incompatible append\n");
			exit(-1);
		}
	}

	if (read(fd[nr_cpu][counter], &read_data, sizeof(read_data)) == -1) {
		perror("Unable to read perf file descriptor\n");
		exit(-1);
	}

	perf_header_attr__add_id(h_attr, read_data.id);

	assert(fd[nr_cpu][counter] >= 0);
	fcntl(fd[nr_cpu][counter], F_SETFL, O_NONBLOCK);

	/*
	 * First counter acts as the group leader:
	 */
	if (group && group_fd == -1)
		group_fd = fd[nr_cpu][counter];
	if (multiplex && multiplex_fd == -1)
		multiplex_fd = fd[nr_cpu][counter];

	if (multiplex && fd[nr_cpu][counter] != multiplex_fd) {
		int ret;

		ret = ioctl(fd[nr_cpu][counter], PERF_EVENT_IOC_SET_OUTPUT, multiplex_fd);
		assert(ret != -1);
	} else {
		event_array[nr_poll].fd = fd[nr_cpu][counter];
		event_array[nr_poll].events = POLLIN;
		nr_poll++;

		mmap_array[nr_cpu][counter].counter = counter;
		mmap_array[nr_cpu][counter].prev = 0;
		mmap_array[nr_cpu][counter].mask = mmap_pages*page_size - 1;
		mmap_array[nr_cpu][counter].base = mmap(NULL, (mmap_pages+1)*page_size,
				PROT_READ|PROT_WRITE, MAP_SHARED, fd[nr_cpu][counter], 0);
		if (mmap_array[nr_cpu][counter].base == MAP_FAILED) {
			error("failed to mmap with %d (%s)\n", errno, strerror(errno));
			exit(-1);
		}
	}

	ioctl(fd[nr_cpu][counter], PERF_EVENT_IOC_ENABLE);
}

static void open_counters(int cpu, pid_t pid)
{
	int counter;

	group_fd = -1;
	for (counter = 0; counter < nr_counters; counter++)
		create_counter(counter, cpu, pid);

	nr_cpu++;
}

static void atexit_header(void)
{
	header->data_size += bytes_written;

	perf_header__write(header, output);
}

static int __cmd_record(int argc, const char **argv)
{
	int i, counter;
	struct stat st;
	pid_t pid = 0;
	int flags;
	int ret;
	unsigned long waking = 0;

	page_size = sysconf(_SC_PAGE_SIZE);
	nr_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	assert(nr_cpus <= MAX_NR_CPUS);
	assert(nr_cpus >= 0);

	atexit(sig_atexit);
	signal(SIGCHLD, sig_handler);
	signal(SIGINT, sig_handler);

	if (!stat(output_name, &st) && st.st_size) {
		if (!force && !append_file) {
			fprintf(stderr, "Error, output file %s exists, use -A to append or -f to overwrite.\n",
					output_name);
			exit(-1);
		}
	} else {
		append_file = 0;
	}

	flags = O_CREAT|O_RDWR;
	if (append_file)
		file_new = 0;
	else
		flags |= O_TRUNC;

	output = open(output_name, flags, S_IRUSR|S_IWUSR);
	if (output < 0) {
		perror("failed to create output file");
		exit(-1);
	}

	if (!file_new)
		header = perf_header__read(output);
	else
		header = perf_header__new();


	if (raw_samples) {
		read_tracing_data(attrs, nr_counters);
	} else {
		for (i = 0; i < nr_counters; i++) {
			if (attrs[i].sample_type & PERF_SAMPLE_RAW) {
				read_tracing_data(attrs, nr_counters);
				break;
			}
		}
	}
	atexit(atexit_header);

	if (!system_wide) {
		pid = target_pid;
		if (pid == -1)
			pid = getpid();

		open_counters(profile_cpu, pid);
	} else {
		if (profile_cpu != -1) {
			open_counters(profile_cpu, target_pid);
		} else {
			for (i = 0; i < nr_cpus; i++)
				open_counters(i, target_pid);
		}
	}

	if (file_new)
		perf_header__write(header, output);

	if (!system_wide) {
		pid_t tgid = pid_synthesize_comm_event(pid, 0);
		pid_synthesize_mmap_samples(pid, tgid);
	} else
		synthesize_all();

	if (target_pid == -1 && argc) {
		pid = fork();
		if (pid < 0)
			perror("failed to fork");

		if (!pid) {
			if (execvp(argv[0], (char **)argv)) {
				perror(argv[0]);
				exit(-1);
			}
		}

		child_pid = pid;
	}

	if (realtime_prio) {
		struct sched_param param;

		param.sched_priority = realtime_prio;
		if (sched_setscheduler(0, SCHED_FIFO, &param)) {
			printf("Could not set realtime priority.\n");
			exit(-1);
		}
	}

	for (;;) {
		int hits = samples;

		for (i = 0; i < nr_cpu; i++) {
			for (counter = 0; counter < nr_counters; counter++) {
				if (mmap_array[i][counter].base)
					mmap_read(&mmap_array[i][counter]);
			}
		}

		if (hits == samples) {
			if (done)
				break;
			ret = poll(event_array, nr_poll, -1);
			waking++;
		}

		if (done) {
			for (i = 0; i < nr_cpu; i++) {
				for (counter = 0; counter < nr_counters; counter++)
					ioctl(fd[i][counter], PERF_EVENT_IOC_DISABLE);
			}
		}
	}

	fprintf(stderr, "[ perf record: Woken up %ld times to write data ]\n", waking);

	/*
	 * Approximate RIP event size: 24 bytes.
	 */
	fprintf(stderr,
		"[ perf record: Captured and wrote %.3f MB %s (~%lld samples) ]\n",
		(double)bytes_written / 1024.0 / 1024.0,
		output_name,
		bytes_written / 24);

	return 0;
}

static const char * const record_usage[] = {
	"perf record [<options>] [<command>]",
	"perf record [<options>] -- <command> [<options>]",
	NULL
};

static const struct option options[] = {
	OPT_CALLBACK('e', "event", NULL, "event",
		     "event selector. use 'perf list' to list available events",
		     parse_events),
	OPT_INTEGER('p', "pid", &target_pid,
		    "record events on existing pid"),
	OPT_INTEGER('r', "realtime", &realtime_prio,
		    "collect data with this RT SCHED_FIFO priority"),
	OPT_BOOLEAN('R', "raw-samples", &raw_samples,
		    "collect raw sample records from all opened counters"),
	OPT_BOOLEAN('a', "all-cpus", &system_wide,
			    "system-wide collection from all CPUs"),
	OPT_BOOLEAN('A', "append", &append_file,
			    "append to the output file to do incremental profiling"),
	OPT_INTEGER('C', "profile_cpu", &profile_cpu,
			    "CPU to profile on"),
	OPT_BOOLEAN('f', "force", &force,
			"overwrite existing data file"),
	OPT_LONG('c', "count", &default_interval,
		    "event period to sample"),
	OPT_STRING('o', "output", &output_name, "file",
		    "output file name"),
	OPT_BOOLEAN('i', "inherit", &inherit,
		    "child tasks inherit counters"),
	OPT_INTEGER('F', "freq", &freq,
		    "profile at this frequency"),
	OPT_INTEGER('m', "mmap-pages", &mmap_pages,
		    "number of mmap data pages"),
	OPT_BOOLEAN('g', "call-graph", &call_graph,
		    "do call-graph (stack chain/backtrace) recording"),
	OPT_BOOLEAN('v', "verbose", &verbose,
		    "be more verbose (show counter open errors, etc)"),
	OPT_BOOLEAN('s', "stat", &inherit_stat,
		    "per thread counts"),
	OPT_BOOLEAN('d', "data", &sample_address,
		    "Sample addresses"),
	OPT_BOOLEAN('n', "no-samples", &no_samples,
		    "don't sample"),
	OPT_BOOLEAN('M', "multiplex", &multiplex,
		    "multiplex counter output in a single channel"),
	OPT_END()
};

int cmd_record(int argc, const char **argv, const char *prefix __used)
{
	int counter;

	argc = parse_options(argc, argv, options, record_usage,
		PARSE_OPT_STOP_AT_NON_OPTION);
	if (!argc && target_pid == -1 && !system_wide)
		usage_with_options(record_usage, options);

	if (!nr_counters) {
		nr_counters	= 1;
		attrs[0].type	= PERF_TYPE_HARDWARE;
		attrs[0].config = PERF_COUNT_HW_CPU_CYCLES;
	}

	for (counter = 0; counter < nr_counters; counter++) {
		if (attrs[counter].sample_period)
			continue;

		attrs[counter].sample_period = default_interval;
	}

	return __cmd_record(argc, argv);
}
