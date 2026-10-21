// Build with -Iheaders; include the standalone tool to test its binary readers.
#define main splitshp_program_main
#include "../splitshp.cc"
#undef main

static FILE* bytes(const unsigned char* data, size_t size) {
	FILE* file = tmpfile();
	assert(file);
	assert(fwrite(data, 1, size, file) == size);
	rewind(file);
	return file;
}

int main(int argc, char** argv) {
	if (argc == 2) {
		// EOF cases run in child processes because the tool exits on read errors.
		if (strcmp(argv[1], "read1") == 0) {
			read1(stdin);
		} else if (strcmp(argv[1], "read2") == 0) {
			read2(stdin);
		} else if (strcmp(argv[1], "read2signed") == 0) {
			read2signed(stdin);
		} else if (strcmp(argv[1], "read4") == 0) {
			read4(stdin);
		} else {
			return 2;
		}
		return 0;
	}
	const unsigned char data[] = {0xff, 0xff, 0xff, 0xff};
	FILE*               file   = bytes(data, sizeof(data));
	assert(read1(file) == 255);
	rewind(file);
	assert(read2(file) == 65535);
	rewind(file);
	assert(read2signed(file) == -1);
	rewind(file);
	assert(read4(file) == 0xffffffffU);
	fclose(file);
	const unsigned char signed_data[] = {0x00, 0x80, 0xff, 0x7f};
	file                              = bytes(signed_data, sizeof(signed_data));
	assert(read2signed(file) == -32768);
	assert(read2signed(file) == 32767);
	fclose(file);
}
