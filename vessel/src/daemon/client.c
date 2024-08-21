
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

#include <base/log.h>
#include <base/env.h>
#include <base/bitmap.h>
#include <base/limits.h>
#include <asm/ops.h>

#include <unistd.h>
#include <stdlib.h>

#include <core/init.h>
#include <dlfcn.h>

#include <daemon/def.h>

extern char** environ;
int environc;
int run_app(int fd, int argc, char** argv, int envc, char **env){
    int ret;
    if ((ret = put_int(argc, fd)) < 0) return ret;
    ret = put_string_vec(argv, fd);
    if (ret < 0 || (size_t) ret != argc) return ret;

    if ((ret=put_int(envc, fd)) < 0) return ret;
    ret = put_string_vec(env, fd);
    if (ret < 0 || (size_t) ret != envc) return ret;
    close(fd);
    return 0;
}

int main(int argc, char **argv) {
	struct sockaddr_un addr;
	int sfd, ret;
    char vessel_control_path[256];
    char *vessel_name = getenv("VESSEL_NAME");
	if(vessel_name == NULL)
	{
		log_err("VESSEL_NAME not set");
		return -1;
	}

    sprintf(vessel_control_path, "/tmp/vessel_%s.sock", vessel_name);
	assert(strlen(vessel_control_path) <= sizeof(addr.sun_path) - 1);
    addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, vessel_control_path, sizeof(addr.sun_path) - 1);
	sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd == -1) {
		log_err("control: socket() failed [%s]", strerror(errno));
		return -errno;
	}
    if(connect(sfd, (struct sockaddr *)&addr,
		 sizeof(struct sockaddr_un)) == -1) {
		log_err("control: bind() failed [%s]", strerror(errno));
		close(sfd);
		return -errno;
	}
    environc = 0;
    for(char **pos=environ; *pos!=NULL; pos++) environc++;
    ret = run_app(sfd, argc, argv, environc, environ);
    log_info("Sended with ret=%d", ret);
    return ret;
}