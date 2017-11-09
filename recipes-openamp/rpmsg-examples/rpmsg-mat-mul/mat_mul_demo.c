/*
 * Sample demo application that showcases inter processor
 * communication from linux userspace to a remote software
 * context. The application generates random matrices and
 * transmits them to the remote context over rpmsg. The
 * remote application performs multiplication of matrices
 * and transmits the results back to this application.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>

#define MATRIX_SIZE 6

struct _matrix {
	unsigned int size;
	unsigned int elements[MATRIX_SIZE][MATRIX_SIZE];
};

static void matrix_print(struct _matrix *m)
{
	int i, j;

	/* Generate two random matrices */
	printf(" \r\n Master : Linux : Printing results \r\n");

	for (i = 0; i < m->size; ++i) {
		for (j = 0; j < m->size; ++j)
			printf(" %d ", (unsigned int)m->elements[i][j]);
		printf("\r\n");
	}
}

static void generate_matrices(int num_matrices,
				unsigned int matrix_size, void *p_data)
{
	int	i, j, k;
	struct _matrix *p_matrix = p_data;
	time_t	t;
	unsigned long value;

	srand((unsigned) time(&t));

	for (i = 0; i < num_matrices; i++) {
		/* Initialize workload */
		p_matrix[i].size = matrix_size;

		printf(" \r\n Master : Linux : Input matrix %d \r\n", i);
		for (j = 0; j < matrix_size; j++) {
			printf("\r\n");
			for (k = 0; k < matrix_size; k++) {

				value = (rand() & 0x7F);
				value = value % 10;
				p_matrix[i].elements[j][k] = value;
				printf(" %d ",
				(unsigned int)p_matrix[i].elements[j][k]);
			}
		}
		printf("\r\n");
	}

}

static pthread_t ui_thread, compute_thread;
static pthread_mutex_t sync_lock;

static int fd, compute_flag;
static int ntimes = 1;
static struct _matrix i_matrix[2];
static struct _matrix r_matrix;

#define RPMSG_GET_KFIFO_SIZE 1
#define RPMSG_GET_FREE_SPACE 3

void *ui_thread_entry(void *ptr)
{
	int cmd, ret, i;

	for (i=0; i < ntimes; i++){
		printf("\r\n **********************************");
		printf("****\r\n");
		printf("\r\n  Matrix multiplication demo Round %d \r\n", i);
		printf("\r\n **********************************");
		printf("****\r\n");
		compute_flag = 1;
		pthread_mutex_unlock(&sync_lock);

		printf("\r\n Compute thread unblocked .. \r\n");
		printf(" The compute thread is now blocking on");
		printf("a read() from rpmsg device \r\n");
		printf("\r\n Generating random matrices now ... \r\n");

		generate_matrices(2, 6, i_matrix);

		printf("\r\n Writing generated matrices to rpmsg ");
		printf("rpmsg device, %d bytes .. \r\n",
				sizeof(i_matrix));

		write(fd, i_matrix, sizeof(i_matrix));

		/* adding this so the threads
		dont overlay the strings they print */
		sleep(1);
		printf("\r\nEnd of Matrix multiplication demo Round %d \r\n", i);
	}

	compute_flag = 0;
	pthread_mutex_unlock(&sync_lock);
	printf("\r\n Quitting application .. \r\n");
	printf(" Matrix multiplication demo end \r\n");

	return 0;
}

void *compute_thread_entry(void *ptr)
{
	int bytes_rcvd;

	pthread_mutex_lock(&sync_lock);

	while (compute_flag == 1) {

		do {
			bytes_rcvd = read(fd, &r_matrix, sizeof(r_matrix));
		} while ((bytes_rcvd < sizeof(r_matrix)) || (bytes_rcvd < 0));

		printf("\r\n Received results! - %d bytes from ", bytes_rcvd);
		printf("rpmsg device (transmitted from remote context) \r\n");

		matrix_print(&r_matrix);

		pthread_mutex_lock(&sync_lock);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	unsigned int size;
	int opt;
	char *rpmsg_dev="/dev/rpmsg0";

	while ((opt = getopt(argc, argv, "dn:")) != -1) {
		switch (opt) {
		case 'd':
			rpmsg_dev = optarg;
			break;
		case 'n':
			ntimes = atoi(optarg);
			break;
		default:
			printf("getopt return unsupported option: -%c\n",opt);
			break;
		}
	}
	printf("\r\n Matrix multiplication demo start \r\n");

	printf("\r\n Open rpmsg dev! \r\n");

	fd = open(rpmsg_dev, O_RDWR);
	if (fd < 0) {
		printf("Failed to open device %s.\n", rpmsg_dev);
		return -1;
	}

	if (pthread_mutex_init(&sync_lock, NULL) != 0)
		printf("\r\n mutex initialization failure \r\n");

	pthread_mutex_lock(&sync_lock);

	printf("\r\n Creating ui_thread and compute_thread ... \r\n");

	pthread_create(&ui_thread, NULL, &ui_thread_entry, "ui_thread");

	pthread_create(&compute_thread, NULL, &compute_thread_entry,
				"compute_thread");
	pthread_join(ui_thread, NULL);

	pthread_join(compute_thread, NULL);

	close(fd);

	printf("\r\n Quitting application .. \r\n");
	printf(" Matrix multiply application end \r\n");

	pthread_mutex_destroy(&sync_lock);

	return 0;
}
