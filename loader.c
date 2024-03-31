#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include "exec_parser.h"

static so_exec_t *exec;
static int fd;
static struct sigaction aux_handler;
static int check;

void segv_handler(int signum, siginfo_t *info, void *context)
{
	if (signum != SIGSEGV) {
		aux_handler.sa_sigaction(signum, info, context);
		return;
	}
	int i;

	check = 1;

	uintptr_t error = (uintptr_t)info->si_addr;

	for (i = 0; i < exec->segments_no; i++) {
		if (exec->segments[i].vaddr + exec->segments[i].mem_size <= error)
			/*the signal doesn't exist in any segment*/
			check = 0;
		else {
			so_seg_t *f_seg = (exec->segments) + i;
			/*alligned address of the page containing the signal*/
			int page = ((error - f_seg->vaddr) / getpagesize()) * getpagesize();

			if (f_seg->data == NULL)
				f_seg->data = calloc(f_seg->mem_size / getpagesize(), sizeof(int));

			/*number of the faulty page*/
			int j = (error - f_seg->vaddr) / getpagesize();

			if (((int *)f_seg->data)[j] != 0) {
				/*if the page is already mapped, run the default handler*/
				aux_handler.sa_sigaction(signum, info, context);
				return;
			}

			char *mapping = mmap((int *)(f_seg->vaddr + page), getpagesize(), PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);

			if (mapping == MAP_FAILED) {
				aux_handler.sa_sigaction(signum, info, context);
				return;
			}

			int cases = getpagesize();
			
			/*
			 * if the mapping was successful, the faulty page's size is
			 * stored in the data array to avoid mapping the same page
			 */
			((int *)f_seg->data)[j] = getpagesize();
			int page_end = page + getpagesize();

			/* treating the 3 cases where the segfault could occure */
			if (page_end > f_seg->file_size && page < f_seg->file_size)
				cases = f_seg->file_size - page;

			if (page >= f_seg->file_size)
				cases = 0;

			if (page < f_seg->file_size && page_end < f_seg->file_size)
				cases = getpagesize();

			/*setting the file's offset and reading the mapped page*/
			lseek(fd, f_seg->offset + ((error - f_seg->vaddr) / getpagesize()) * getpagesize(), SEEK_SET);
			read(fd, mapping, cases);
			/*setting the original permissions of the segment*/
			mprotect(mapping, getpagesize(), f_seg->perm);
			check = 1;
			break;
		}
	}
	if (check == 0) {
		aux_handler.sa_sigaction(signum, info, context);
		return;
	}

}

int so_init_loader(void)
{
	int rc;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	rc = sigaction(SIGSEGV, &sa, &aux_handler);
	if (rc < 0) {
		perror("sigaction");
		return -1;
	}
	return 0;
}

int so_execute(char *path, char *argv[])
{
	fd = open(path, O_RDONLY);
	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	so_start_exec(exec, argv);

	close(fd);
	return -1;
}
