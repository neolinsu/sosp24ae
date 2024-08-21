#pragma once

#define CMD_GET 0x00
#define CMD_SET 0x01

typedef struct __attribute__ ((__packed__)) {
	uint8_t magic;
	uint8_t opcode;
	uint16_t key_len;
	uint8_t extra_len;
	uint8_t data_type;
	union {
		uint16_t vbucket; // request use
		uint16_t status;  // response use
	};
	uint32_t body_len;
	uint32_t opaque;
	uint64_t version;
} binary_header_t;

enum conn_state {
	STATE_HEADER = 1,
	STATE_EXTRA,
	STATE_KEY,
	STATE_VALUE,
	STATE_PROC,
	STATE_RESPONSE,
};

struct __attribute__ ((__packed__)) mc_header {
	uint16_t req_id;
	uint16_t seq_no;
	uint16_t dat_no;
	uint16_t fu;
};

struct __attribute__ ((__packed__)) udp_response_header {
	struct mc_header mc_h;
	binary_header_t req_h;
};
