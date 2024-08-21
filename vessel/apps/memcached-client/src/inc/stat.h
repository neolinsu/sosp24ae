#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <client.h>
#include <quadsort.h>
#include <config.h>


void get_lat(
    uint64_t client_num,
    uint64_t req_num_per_client,
    struct req** list,
    uint64_t **lat_out)
{
    uint64_t ele_num = client_num * req_num_per_client;
    uint64_t* lat = (uint64_t*)malloc(sizeof(uint64_t) * ele_num);
    uint64_t idx = 0;
    uint64_t fail_cnt = 0;
    for (uint64_t c=0; c<client_num; ++c) {
        struct req* clist = list[c];
        for (uint64_t q=0; q<req_num_per_client; ++q) {
            if (clist[q].real_end_tsc == 0) {
                lat[idx] = 0;
                fail_cnt++;
            } else {
                lat[idx] = clist[q].real_end_tsc - clist[q].real_start_tsc;
            }
            idx++;
        }
    }

    double check_points[6] = {25.0, 50.0, 75.0, 90.0, 99.0, 99.9};
    size_t point_idx = 0;
    uint64_t sort_fail_cnt = 0;
    quadsort_prim(lat, ele_num, sizeof(uint64_t));
    for (uint64_t q=0; q<6; q++) {
        printf("%f\t", check_points[q]);
    }
    printf("\n");
    for (uint64_t q=0; q<ele_num; q++) {
        if (lat[q] == 0) {
            sort_fail_cnt++;
            continue;
        }
        double cur_point = (double)(q-sort_fail_cnt)*100.0/(ele_num-sort_fail_cnt);
        if (cur_point > check_points[point_idx]) {
            printf("%f\t", (double)lat[q]/clock);
            point_idx++;
            if (point_idx>=6) {
                break;
            }
        }
    }
    printf("\n");
    printf("Never send: %lf%%\n", (double)fail_cnt *100.0 / ele_num);
    *lat_out = lat;
}
