/* Standalone config-reader regression test; link with SDL3. */
#define MAIN
#include "../smooth/config.c"

#include <assert.h>

static int loads, inits, unloads, parses;
static int fail_load, fail_init;
static char last_plugin[MAX_LINE_LENGTH + 16];
static node handle_node;

static void* init_stub(glob_statics* globals) {
	assert(globals == &g_statics);
	++inits;
	return NULL;
}

libhandle_t plug_load(char* name) {
	++loads;
	snprintf(last_plugin, sizeof(last_plugin), "%s", name);
	return fail_load ? NULL : (libhandle_t)&handle_node;
}

void* plug_load_func(libhandle_t handle, const char* name) {
	assert(handle == (libhandle_t)&handle_node);
	assert(strcmp(name, "init_plugin") == 0);
	void* (*init)(glob_statics*) = init_stub;
	void* result;
	memcpy(&result, &init, sizeof(result));
	return fail_init ? NULL : result;
}

int plug_unload(libhandle_t handle) {
	assert(handle == (libhandle_t)&handle_node);
	++unloads;
	return 1;
}

node* add_handle(libhandle_t handle, node* list) {
	(void)list;
	assert(handle == (libhandle_t)&handle_node);
	return &handle_node;
}

int add_plugin_parse(char* line, libhandle_t handle) {
	assert(strcmp(line, "112233 445566") == 0);
	assert(handle == (libhandle_t)&handle_node);
	++parses;
	return 0;
}

int add_plugin_apply(int index, libhandle_t handle) {
	(void)index;
	assert(handle == (libhandle_t)&handle_node);
	return 0;
}

static int parse(const char* input) {
	FILE* file = tmpfile();
	assert(file != NULL);
	assert(fputs(input, file) >= 0);
	loads = inits = unloads = parses = 0;
	last_plugin[0] = '\0';
	int result = read_config(file);
	assert(fclose(file) == 0);
	return result;
}

int main(void) {
	g_statics.image_in_format = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGB24);
	assert(parse("") == 0 && loads == 0);
	assert(parse("# comment\n; comment\n\n") == 0 && loads == 0);
	const char* valid[] = {"[test]", "[test]\n", "[test]\r\n"};
	for (size_t i = 0; i < sizeof(valid) / sizeof(*valid); ++i) {
		assert(parse(valid[i]) == 0 && loads == 1 && inits == 1);
#ifdef _WIN32
		assert(strcmp(last_plugin, "libsmooth_test.dll") == 0);
#else
		assert(strcmp(last_plugin, "libsmooth_test.so") == 0);
#endif
	}
	const char* invalid[] = {"[", "[\n", "[]", "[test", "[test]trailing"};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
		assert(parse(invalid[i]) == -1 && loads == 0);
	}
	assert(parse("[test]\n112233 445566") == 0 && parses == 1);
	assert(parse("[test]\ninvalid color") == -1);
	char too_long[MAX_LINE_LENGTH + 16];
	memset(too_long, 'x', sizeof(too_long) - 1);
	too_long[sizeof(too_long) - 1] = '\0';
	assert(parse(too_long) == -1);
	fail_load = 1;
	assert(parse("[test]") == -1 && inits == 0);
	fail_load = 0;
	fail_init = 1;
	assert(parse("[test]") == -1 && inits == 0 && unloads == 1);
	return 0;
}
