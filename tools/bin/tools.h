/*
 * Copyright 2023 wtcat
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#include <string>
#include <vector>


namespace tools {

static inline uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len) {
	/* crc table generated from polynomial 0xedb88320 */
	static const uint32_t table[16] = {
		0x00000000U, 0x1db71064U, 0x3b6e20c8U, 0x26d930acU,
		0x76dc4190U, 0x6b6b51f4U, 0x4db26158U, 0x5005713cU,
		0xedb88320U, 0xf00f9344U, 0xd6d6a3e8U, 0xcb61b38cU,
		0x9b64c2b0U, 0x86d3d2d4U, 0xa00ae278U, 0xbdbdf21cU,
	};

	crc = ~crc;
	for (size_t i = 0; i < len; i++) {
		uint8_t byte = data[i];
		crc = (crc >> 4) ^ table[(crc ^ byte) & 0x0f];
		crc = (crc >> 4) ^ table[(crc ^ ((uint32_t)byte >> 4)) & 0x0f];
	}
	return ~crc;
}

class cc_file_packge {
public:
    enum {
        kNameMaxLen = 16
    };

    cc_file_packge(int argc, char *argv[]) {
        parse(argc, argv);
    };
    cc_file_packge() = default;
    virtual ~cc_file_packge() = default;
    virtual bool parse(int argc, char *argv[]);
    bool merge_bin(void);

protected:
    virtual void filter(const std::string &name, void *buffer, size_t size) {}
    virtual void show_usage() const;

private:
    bool get_flist(const std::string& filter);

protected:
    std::string dir_;
    std::string ofile_;

private:
    std::vector<std::string> flist_;
};

} //namespace tools