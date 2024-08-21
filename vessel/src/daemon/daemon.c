
#include <sys/eventfd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/mman.h>

#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <execinfo.h>
#include <signal.h>


#include <base/log.h>
#include <base/env.h>
#include <base/bitmap.h>
#include <base/limits.h>
#include <asm/ops.h>

#include <unistd.h>
#include <stdlib.h>

#include <core/init.h>
#include <core/kthread.h>
#include <core/config.h>
#include <dlfcn.h>

#include <daemon/def.h>

extern kthread_fs_map_t  *global_kthread_fs_map;
extern int force_init_vessel_meta; 

cpu_set_t vessel_cpuset;

int argc;
char **argv;
int envc;
char **env;
char *tty_path;
spinlock_t argsl;
DEFINE_BITMAP(input_allowed_cores, NCPU);

static int read_data(int fd) {
  int ret;
  if ((ret=get_int(&argc, fd)) < 0) return ret;
  ret = get_string_vec(&argv, fd);
  if (ret < 0 || (size_t) ret != argc) return ret;

  if ((ret=get_int(&envc, fd)) < 0) return ret;
  ret = get_string_vec(&env, fd);
  if (ret < 0 || (size_t) ret != envc) return ret;

  return ret;
}

int run_app(int client_fd) {
  ssize_t ret;
  
  ret = read_data(client_fd);
  if (ret<0) {
    log_err("D: Fail to read_data");
    return -EACCES;
  }
  DEFINE_BITMAP(cpu_aff, NCPU);
  bitmap_init(cpu_aff, NCPU, false);
  if (string_to_bitmap(argv[1], cpu_aff, NCPU))
  {
    log_err("Invalid cpu list: %s\n", argv[1]);
    log_err("Example list: 0-24,26-48:2,49-255\n");
    return -ENAVAIL;
  }
  ret = pthread_setaffinity_np(pthread_self(), sizeof(vessel_cpuset), &vessel_cpuset);
  if (ret) {
    log_err("Fail to pthread_setaffinity_np for app");
    return -ENAVAIL;
  }
  ret = task_init(cpu_aff, 0, argv[2], argc - 2, argv + 2, env, VESSEL_USER_ID);
  return ret;
}

static int control_worker(void)
{
	struct sockaddr_un addr;
	int sfd, ret;

  char vessel_control_path[107];
  sprintf(vessel_control_path, "/tmp/vessel_%s.sock", vessel_name);
	assert(strlen(vessel_control_path) <= sizeof(addr.sun_path) - 1);
	
	memset(&addr, 0x0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, vessel_control_path, sizeof(addr.sun_path) - 1);

	sfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sfd == -1) {
		log_err("control: socket() failed [%s]", strerror(errno));
		return -errno;
	}
	unlink(vessel_control_path);
	if (bind(sfd, (struct sockaddr *)&addr,
		 sizeof(struct sockaddr_un)) == -1) {
		log_err("control: bind() failed [%s]", strerror(errno));
		close(sfd);
		return -errno;
	}
  ret = chmod(vessel_control_path, 0666);
  if(ret) {
		log_err("control: chmod() failed[%s]", strerror(errno));
    close(sfd);
    return -errno;
  }
	if (listen(sfd, 100) == -1) {
		log_err("control: listen() failed[%s]", strerror(errno));
		close(sfd);
		return -errno;
	}
  while(1) {
    int client_fd = accept(sfd, NULL, NULL);
    if (client_fd == -1) {
      sleep(2000);
      continue;
    }
    spin_lock(&argsl);
    ret = run_app(client_fd);
    BUG_ON(ret);
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    ret = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    spin_unlock(&argsl);
    close(client_fd);
  }
  if (sfd) close(sfd);
	return 0;
}


int main(int argc, char **argv)
{
  int ret;
  if (argc < 2) {
		fprintf(stderr, "need cpu list.\n");
    goto PARSE_CPU_FAIL;
  } else if (string_to_bitmap(argv[1], input_allowed_cores, NCPU)) {
    fprintf(stderr, "invalid cpu list: %s\n", argv[1]);
    goto PARSE_CPU_FAIL;
  }
  goto SUCCESS;
PARSE_CPU_FAIL:
	fprintf(stderr, "example list: 0-24,26-48:2,49-255\n");
  return EINVAL;
SUCCESS:
  int cpu;
  bitmap_for_each_set(input_allowed_cores, NCPU, cpu) {
    CPU_SET(cpu, &vessel_cpuset);
  }
  ret = pthread_setaffinity_np(pthread_self(), sizeof(vessel_cpuset), &vessel_cpuset);
  ret = core_init();
  if (ret) {
    log_err("Fail to core init."); 
    return ret;
  }
  log_info("Avaliable CPUs:");
  for (int j = 0; j < 1024; j++)
    if (CPU_ISSET(j, &vessel_cpuset))
        printf("%d,", j);
  puts("\n------------");
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(0, &cpuset);
  ret = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
  BUG_ON(ret);
  ret = control_worker();
  log_err("daemon exit for %d", ret);
  return ret;
}