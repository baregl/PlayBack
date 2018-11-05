#include <config.h>
#include <syncer.h>

#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int tcp_connection = -1;
int open_file = -1;
int rand_file = -1;

static const int PORT = 9483;

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage:\n%s config\n", argv[0]);
		exit(-1);
	}

	rand_file = open("/dev/urandom", O_RDONLY);
	if (rand_file < -1) {
		printf("Couldn't open urandom\n");
		exit(-1);
	}

	config_data *config = config_parse(argv[1]);

	// TCP Open
	struct addrinfo hints, *infoptr;
	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	char portbuf[6];
	sprintf(portbuf, "%d", PORT);
	if (getaddrinfo(config->server, portbuf, &hints, &infoptr))
		clbk_show_error("Couldn't resolve server address");

	int cret = -1;
	for (struct addrinfo *p = infoptr; p != NULL; p = p->ai_next) {
		tcp_connection =
		    socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (tcp_connection == -1) {
			continue;
		}
		if ((cret = connect(tcp_connection, p->ai_addr,
				    p->ai_addrlen)) < 0) {
			continue;
		}
		break;
	}
	if (tcp_connection == -1 || cret == -1)
		clbk_show_error("Connect to server failed");
	LOG("Starting sync\n");
	syncer_run(config, "samplecl", "0.1");

	close(open_file);
	close(rand_file);
	close(tcp_connection);
}

// Implementation should open tcp connection befor calling syncer_run
// If connection is closed prematurely, display error
void clbk_send(uint8_t *data, uint32_t length)
{
	write(tcp_connection, data, length);
}

uint32_t clbk_receive(uint8_t *data, uint32_t length)
{
	int ret = read(tcp_connection, data, length);
	if (ret == -1)
		clbk_show_error("Read error");
	return ret;
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
	;
	return (dird->dir == NULL) ? NULL : dird;
}

void clbk_close_dir(void *dird_void)
{
	struct dir_desc *dird = (struct dir_desc *)dird_void;
	closedir(dird->dir);
	free(dird->path);
	free(dird);
}

// allocate fodlre_entry statically
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
		char name[strlen(dird->path) + strlen(dirret->d_name) + 2];
		sprintf(name, "%s%s", dird->path, dirret->d_name);
		struct stat statbuffer;
		if (stat(name, &statbuffer) == -1) {
			clbk_show_status("Couldn't stat ");
			clbk_show_status(name);
			clbk_show_error(" for readdir\n");
		}
		dire.size = statbuffer.st_size;
		dire.mtime = statbuffer.st_mtime;
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
	printf("%s\n", msg);
	if (open_file != -1)
		close(open_file);
	if (tcp_connection != -1)
		close(tcp_connection);
	if (rand_file != -1)
		close(rand_file);
	exit(1);
}

void clbk_show_status(char *status)
{
	printf("%s", status);
}

int clbk_config_entry(char *key, char *val)
{
	return -1;
}

void clbk_delay(uint8_t ms)
{
	if (usleep(1000000 * ms) != 0)
		clbk_show_status("Sleep error");
}

void clbk_get_random(uint8_t *data, uint8_t len)
{
	if (read(rand_file, data, len) != len)
		clbk_show_error("Couldn't read urandom");
}
