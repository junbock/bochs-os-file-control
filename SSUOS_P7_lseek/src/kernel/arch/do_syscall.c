#include <proc/sched.h>
#include <proc/proc.h>
#include <device/device.h>
#include <interrupt.h>
#include <device/kbd.h>
#include <filesys/file.h>
#include <syscall.h>

pid_t do_fork(proc_func func, void* aux1, void* aux2)
{
	pid_t pid;
	struct proc_option opt;

	opt.priority = cur_process-> priority;
	pid = proc_create(func, &opt, aux1, aux2);

	return pid;
}

void do_exit(int status)
{
	cur_process->exit_status = status; 	//종료 상태 저장
	proc_free();						//프로세스 자원 해제
	do_sched_on_return();				//인터럽트 종료시 스케줄링
}

pid_t do_wait(int *status)
{
	while(cur_process->child_pid != -1)
		schedule();
	//SSUMUST : schedule 제거.

	int pid = cur_process->child_pid;
	cur_process->child_pid = -1;

	extern struct process procs[];
	procs[pid].state = PROC_UNUSED;

	if(!status)
		*status = procs[pid].exit_status;

	return pid;
}

void do_shutdown(void)
{
	dev_shutdown();
	return;
}

int do_ssuread(void)
{
	return kbd_read_char();
}

int do_open(const char *pathname, int flags)
{
	struct inode *inode;
	struct ssufile **file_cursor = cur_process->file;
	int fd;

	for(fd = 0; fd < NR_FILEDES; fd++)
		if(file_cursor[fd] == NULL) break;

	if (fd == NR_FILEDES)
		return -1;

	if ( (inode = inode_open(pathname)) == NULL)
		return -1;
	
	if (inode->sn_type == SSU_TYPE_DIR)
		return -1;

	fd = file_open(inode,flags,0);
	
	return fd;
}

int do_read(int fd, char *buf, int len)
{
	return generic_read(fd, (void *)buf, len);
}
int do_write(int fd, const char *buf, int len)
{
	return generic_write(fd, (void *)buf, len);
}
int do_lseek(int fd, int offset, int whence)
{
	struct ssufile *cursor;
	int temp;
	int i, n, start, size;
	char c;
	if( (cursor = cur_process->file[fd]) == NULL)
		return -1;
	//null opt
	if (whence >= -1 && whence <= 1)
	{
		//whence에서 offset만큼 이동
		switch (whence) {
			case SEEK_SET:
				temp = offset;
				break;
			case SEEK_CUR:
				temp = cursor->pos + offset;
				break;
			case SEEK_END:
				temp = cursor->inode->sn_size + offset;
				break;
		}
		//음수이거나 파일크기를 넘는 값은 -1 리턴
		if (temp < 0)
			return -1;
		else if (temp > cursor->inode->sn_size)
			return -1;
		else
		{
			//정상값이면 cursor->pos 변경 후 리턴
			cursor->pos = temp; 
			return cursor->pos;
		}
	}
	//e opt
	if (whence >= 9 && whence <= 11)
	{
		//whence에서 offset만큼 이동
		switch (whence-10) {
			case SEEK_SET:
				temp = offset;
				break;
			case SEEK_CUR:
				temp = cursor->pos + offset;
				break;
			case SEEK_END:
				temp = cursor->inode->sn_size + offset;
				break;
		}
		//음수 값 -1 리턴
		if (temp < 0)
			return -1;
		else if (temp > cursor->inode->sn_size)
		{	
			//파일 크기를 넘어간 값만큼 0으로 집어넣음
			cursor->pos = cursor->inode->sn_size;
			n = temp - cursor->inode->sn_size;
			for (i = 0; i < n; i++)
				write(fd, "0", 1);
			return cursor->pos;
		} 	
		else
		{
			cursor->pos = temp; 
			return cursor->pos;
		}
	}
	//a opt
	if (whence >= 99 && whence <= 101)
	{
		//whence에서 offset만큼 이동한 값
		switch (whence-100) {
			case SEEK_SET:
				start = 0;
				break;
			case SEEK_CUR:
				temp = cursor->pos + offset;
				if (offset < 0)
					start = cursor->pos + offset;
				else 
					start = cursor->pos;
				break;
			case SEEK_END:
				temp = cursor->inode->sn_size + offset;
				if (offset < 0)
					start = cursor->pos + offset;
				else 
					start = cursor->pos;
				break;
		}
		if (offset < 0)
			offset = -offset;
		if (start < 0)
			start = 0;
		//옮겨야할 시작점
		n = cursor->inode->sn_size - start;
		//파일사이즈 늘리기 전의 사이즈저장
		size = cursor->inode->sn_size;

		//파일사이즈 offset만큼 늘리기		
		cursor->pos = cursor->inode->sn_size;
		for (i = 0; i < offset; i++)
			write(fd, "\0", 1);
		//파일 끝에서부터 옮겨야할 시작점까지 offset만큼 옮기기
		for (i = 0; i < n; i++)
		{
			cursor->pos = size - 1 - i;
			read(fd, &c, 1);
			cursor->pos += offset - 1;
			write(fd, &c, 1);	
		}
		//offset만큼 0채우기
		cursor->pos = start;
		for(i = 0; i < offset; i++)
			write(fd, "0", 1);
		cursor->pos = start;
		return cursor->pos;
	}
	//re opt	
	if (whence >= 999 && whence <= 1001)
	{
		//whence에서 offset만큼 이동한 값
		switch (whence-1000) {
			case SEEK_SET:
				temp = offset;
				break;
			case SEEK_CUR:
				temp = cursor->pos + offset;
				break;
			case SEEK_END:
				temp = cursor->inode->sn_size + offset;
				break;
		}
		//음수 값
		if (temp < 0)
		{
			if (offset < 0)
				offset = -offset;
			
			size = cursor->inode->sn_size;
			//파일사이즈 offset만큼 늘리기
			cursor->pos = cursor->inode->sn_size;
			for (i = 0; i < offset; i++)
				write(fd, "\0", 1);
			//끝부터 처음까지 offset만큼 옮기기
			for (i = 0; i < size; i++)
			{
				cursor->pos = size - 1 - i;
				read(fd, &c, 1);
				cursor->pos += offset - 1;
				write(fd, &c, 1);
			}
			//offset만큼 0으로 채우기
			cursor->pos = 0;
			for (i = 0; i < offset; i++)
				write(fd, "0", 1);
			cursor->pos = 0;
			return cursor->pos;	
			
		}
		else if (temp > cursor->inode->sn_size)
			return -1;	
		else
		{
			cursor->pos = temp; 
			return cursor->pos;
		}
	}
	//c opt	
	if (whence >= -9999 && whence <= 10001)
	{
		switch (whence-10000) {
			case SEEK_SET:
				temp = offset;
				break;
			case SEEK_CUR:
				temp = cursor->pos + offset;
				break;
			case SEEK_END:
				temp = cursor->inode->sn_size + offset;
				break;
		}
		size = cursor->inode->sn_size;
		//음수값 양수로 바꿈		
		if (temp < 0)
			while (temp < 0)
				temp += size;
		//모듈러		
		cursor->pos = temp % size;
		return cursor->pos;
		
	}
			
}
