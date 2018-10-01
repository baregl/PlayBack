#include <config.h>
#include <syncer.h>

#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <malloc.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <3ds.h>

#define SOC_ALIGN 0x1000
#define SOC_BUFFERSIZE 0x100000

static uint32_t *SOC_buffer = NULL;

char buffer[1024 * 1024];

int tcp_connection = -1;
int open_file = -1;

static const int PORT = 9483;
char *server;
char *pass;
char *name;
char **dirs = NULL;
int dirs_len = 0;

void socShutdown(void);
void apt_thread(void *);
void add_dir(char *dir, char ***dirs, int *dirs_len);

int main(void)
{
	int ret;
	gfxInitDefault();

	// register gfxExit to be run when app quits
	// this can help simplify error handling
	atexit(gfxExit);

	consoleInit(GFX_TOP, NULL);

	printf("\nembsyncclient\n");

	// allocate buffer for SOC service
	SOC_buffer = (uint32_t *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);

	if (SOC_buffer == NULL) {
		clbk_show_error("memalign: failed to allocate\n");
	}

	// Now intialise soc:u service
	if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
		printf("socInit: 0x%08X\n", (unsigned int)ret);
		clbk_show_error("Couldn't initialize soc:u service");
	}

	// register socShutdown to run at exit
	// atexit functions execute in reverse order so this runs before gfxExit
	atexit(socShutdown);

	// Start apt thread, so we can exit
	threadCreate(apt_thread, NULL, 0x1000, 0x30, -1, true);

	if (config_parse("sdmc:/plybck.cfg") != 0) {
		printf("Couldn't parse config\n");
		exit(-1);
	}
	if (server == NULL || pass == NULL || dirs == NULL || name == NULL) {
		printf("Missing config parameters\n");
	}
	LOG("Opening TCP Connection\n");
	tcp_connection = socket(PF_INET, SOCK_STREAM, 0);
	struct hostent *host;
	if ((host = gethostbyname(server)) == NULL) {
		clbk_show_error("Couldn't resove server");
	}
	int cret = connect(tcp_connection,
			   (const struct sockaddr *)&((struct sockaddr_in){
			       .sin_family = AF_INET,
			       .sin_port = htons(PORT),
			       .sin_addr.s_addr = *(long *)(host->h_addr)}),
			   sizeof(struct sockaddr_in));

	if (tcp_connection == -1 || cret == -1)
		clbk_show_error("Connect to server failed");
	LOG("Starting sync\n");
	syncer_run(dirs, "3ds", name, "0.1", pass);

	close(open_file);
	shutdown(tcp_connection, SHUT_RDWR);
	close(tcp_connection);
}

void apt_thread(void *data)
{
	while (aptMainLoop())
		svcSleepThread(10000000);
	exit(0);
}

// Implementation should open tcp connection befor calling syncer_run
// If connection is closed prematurely, display error
void clbk_send(uint8_t *data, uint32_t length)
{
	write(tcp_connection, data, length);
}

uint32_t clbk_receive(uint8_t *data, uint32_t length)
{
	return read(tcp_connection, data, length);
}

// Open file for reading, close previous
int clbk_open(char *path)
{
	printf("Opening %s\n", path);
	if (open_file != -1)
		close(open_file);
	open_file = open(path, O_RDONLY);
	if (open_file == -1) {
	}
	return (open_file == -1) ? open_file : 0;
}

uint32_t clbk_read(uint8_t *data, uint32_t length)
{
	return read(open_file, data, length);
}

struct dir_desc {
	DIR *dir;
	char *path;
};

// The path is absolute
void *clbk_open_dir(char *path)
{
	struct dir_desc *dird = malloc(sizeof(struct dir_desc));
	dird->dir = opendir(path);
	dird->path = malloc(strlen(path) + 1);
	memcpy(dird->path, path, strlen(path) + 1);
	return (dird->dir == NULL) ? NULL : dird;
}

void clbk_close_dir(void *dird_void)
{
	struct dir_desc *dird = (struct dir_desc *)dird_void;
	closedir(dird->dir);
	free(dird->path);
	free(dird);
}

// allocate folder_entry statically
struct dir_entry *clbk_read_dir(void *dird_void)
{
	struct dir_desc *dird = (struct dir_desc *)dird_void;
	static struct dir_entry dire;
	struct dirent *dirret = readdir(dird->dir);
	if (dirret == NULL)
		return NULL;
	switch (dirret->d_type) {
	case DT_REG: {
		dire.dir = false;
		// The sdmc directory entry already has the size, so no need to
		// do a stat, especially since stat doesn't contain the mtime
		sdmc_dir_t *dir = (sdmc_dir_t *)dird->dir->dirData->dirStruct;

		if (*(uint32_t *)dir != SDMC_DIRITER_MAGIC)
			clbk_show_error("Dir doesn't have diriter?");

		FS_DirectoryEntry *entry = &dir->entry_data[dir->index];
		dire.size = entry->fileSize;
		char name[strlen(dird->path) + strlen(dirret->d_name) + 1];
		sprintf(name, "%s%s", dird->path, dirret->d_name);
		if (sdmc_getmtime(name, &dire.mtime) != 0) {
			clbk_show_status("Couldn't get mtime for");
			clbk_show_error(name);
		}
	} break;
	case DT_DIR:
		dire.dir = true;
		dire.size = 0;
		dire.mtime = 0;
		break;
	default:
		clbk_show_error("Invalid file in dir\n");
	}
	dire.name = dirret->d_name;
	return &dire;
}

uint32_t clbk_file_size(char *path)
{
	struct stat statbuffer;
	if (stat(path, &statbuffer) == -1)
		clbk_show_error("Couldn't stat file");

	return statbuffer.st_size;
}

// Exit in this function
void clbk_show_error(char *msg)
{
	printf(CONSOLE_RED);
	printf("%s\n", msg);
	printf(CONSOLE_RESET);
	if (open_file != -1)
		close(open_file);
	if (tcp_connection != -1)
		close(tcp_connection);
	printf("\nPress B to exit\n");

	while (aptMainLoop()) {
		gspWaitForVBlank();
		hidScanInput();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_B)
			exit(0);
	}
}

void clbk_show_status(char *status)
{
	printf("%s", status);
}

void clbk_config_entry(char *key, char *val)
{
	int len = strlen(val) + 1;
	char *data = malloc(len);
	memcpy(data, val, len);
	if (strcmp("dir", key) == 0)
		add_dir(data, &dirs, &dirs_len);
	else if (strcmp("pass", key) == 0)
		pass = data;
	else if (strcmp("name", key) == 0)
		name = data;
	else if (strcmp("server", key) == 0)
		server = data;
	else {
		free(data);
		printf("Unknown key %s\n", key);
	}
}

void socShutdown(void)
{
	if (open_file != -1)
		close(open_file);
	if (tcp_connection != -1)
		close(tcp_connection);
}

void add_dir(char *dir, char ***dirs, int *dirs_len)
{
	*dirs = realloc(*dirs, (*dirs_len + 2) * sizeof(**dirs));
	(*dirs)[(*dirs_len)++] = dir;
	(*dirs)[*dirs_len] = 0;
}
