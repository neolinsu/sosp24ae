#pragma once
#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <base/log.h>

#include "cpu_utils.h"


static inline int read_dir_list(char *basePath, char *res[])
{
    DIR *dir;
    struct dirent *ptr;
	int cnt = 0;

    if ((dir=opendir(basePath)) == NULL)
    {
        log_err("Open dir error...");
        return -EACCES;
    }

    while ((ptr=readdir(dir)) != NULL)
    {
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0)    ///current dir OR parrent dir
            continue;
        else if(ptr->d_type == 4)
        {
			char * dir_name = (char*) malloc(sizeof(char) * PATH_MAX);
			memset(dir_name, 0, sizeof(char) * PATH_MAX);
			strcpy(dir_name, ptr->d_name);
			res[cnt++] = dir_name;
        }
    }
    closedir(dir);
    return cnt;
}

static inline int dp_pin_thread(pid_t pid, pid_t mytid, int mycore, int otherscore)
{
	cpu_set_t cpuset;
	int ret, cnt;
	char *tid_strs[PATH_MAX];
	char *path_to_tids = (char*)malloc(sizeof(char) * PATH_MAX);

	sprintf(path_to_tids, "/proc/%d/task/", pid);
	cnt = read_dir_list(path_to_tids, tid_strs);
	BUG_ON(cnt<=1);
	for (int i=0;i<cnt;++i) {
		pid_t tid;
		sscanf(tid_strs[i], "%d\n", &tid);
		if (tid != mytid) {
			CPU_ZERO(&cpuset);
			CPU_SET(otherscore, &cpuset);
			ret = sched_setaffinity(tid, sizeof(cpu_set_t), &cpuset);
			if (ret < 0) {
				log_err("cores: failed to set affinity for thread %d with err %d",
					tid, errno);
				return -errno;
			}
		} else {
			CPU_ZERO(&cpuset);
			CPU_SET(mycore, &cpuset);
			ret = sched_setaffinity(tid, sizeof(cpu_set_t), &cpuset);
			if (ret < 0) {
				log_err("cores: failed to set affinity for thread %d with err %d",
					tid, errno);
				return -errno;
			}
		}
	}
	return 0;
}