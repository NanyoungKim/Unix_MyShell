#include <string.h>
#include <unistd.h>   //fork()가 system call 로 정의되어 있음
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>
#define MAX_CMD_ARG 10

const char *prompt = "myshell> ";
char* cmdvector[MAX_CMD_ARG];
char* cmdvector2[MAX_CMD_ARG];
char* cmdvector3[MAX_CMD_ARG];
char  cmdline[BUFSIZ];
char  cmdline2[BUFSIZ];
pid_t pid = -1;  //process id를 저장하는 type. 시스템마다 프로세스 번호의 타입이 다름.
            //범위는 2~32768임.(0번은 부팅 후 바로 사라지고 1번은 모든 프로세스의 부모인 init프로세스)

int p[2], status;
void fatal(char *str){      //심각한 오류 방지
    perror(str);
    exit(1);
}

//makelist() : 구분자 delimiters를 가지고, 문자열 s 로부터 list를 인자 배열로 만든다. 성공시 토큰 수 리턴, 실패 시 -1 리턴
int makelist(char *s, const char *delimiters, char** list, int MAX_LIST){   //하나의 커맨드라인 인자를 가지고 해당 함수 호출하여 인자 배열을 만듦.
  int i = 0;
  int numtokens = 0;
  char *snew = NULL;

  if( (s==NULL) || (delimiters==NULL) ) return -1;  //실패 시 -1 리턴

  snew = s + strspn(s, delimiters);    /* delimiters∏¶ skip */    //strspn(): 문자열 앞의 공백 제거, snew는 문자열의 실제 시작 지점
  if( (list[numtokens]=strtok(snew, delimiters)) == NULL )        //strtok() : 문자열 snew를 구분자를 기준으로 토큰으로 나눔.
    return numtokens;

  numtokens = 1;

  while(1){
     if( (list[numtokens]=strtok(NULL, delimiters)) == NULL)    break;
     if(numtokens == (MAX_LIST-1)) return -1;
     numtokens++;
  }

  return numtokens;
}

void cmd_cd(int argc, char** argv) {
    if (argc == 1)                      //cd만 입력 받은 경우 HOME으로 change directory
        chdir(getenv("HOME"));
    else if (argc == 2) {               //cd dirA -> dirA로 change directory
        if (chdir(argv[1]))
            printf("No directory\n");       //해당 디렉토리 없는 경우
    }
    else
        printf("error\n");
}


void sigchldhandler(int signal){
    wait(NULL);                         //자식 프로세스의 종료를 기다림.
}
void sighandler(int signal) {
    if(pid == 0) {exit(0);}                      //자식 프로세스일 때만 제어키에 의해 종료됨.
}

/* redirection in 함수 */
int redirection_in(char **cmdvector){
	int idx = 0, fd = 0;

	while(cmdvector[idx] != NULL){
		if(strcmp(cmdvector[idx], "<") == 0)		//cmdvector에서 <의 인덱스 찾기
			break;
		idx++;
	}

	if(idx == 0 || !cmdvector[idx + 1])
		return -1;

	if((fd = open(cmdvector[idx+1], O_RDONLY)) == -1){
		fatal("(redirection_in)");
		return -1;
	}
	dup2(fd, 0);    //표준 입력을 fd 가 가리키는 파일로 대체.
	close(fd);

	while(cmdvector[idx] != NULL){
		cmdvector[idx] = cmdvector[idx + 2];		//'< ' 다음 내용 정리
		idx++;
	}
	cmdvector[idx] = NULL;

	return 0;
}

int redirection_out(char **cmdvector){
	int idx = 0;
	int fd = 0;

	while(cmdvector[idx] != NULL){
		if(strcmp(cmdvector[idx], ">") == 0){		//cmdvector에서 >의 인덱스 찾기
			break;
		}
		idx++;
	}

	if(idx == 0 || !cmdvector[idx + 1]){
		return -1;
	}
	/* redirection */
	if((fd = open(cmdvector[idx+1], O_RDWR|O_CREAT,0644)) == -1){		//> 이후의 명령어에 대해 파일 오. 이 파일에 출력할 것.
		fatal("(redirection_out)");
		return -1;
	}

	dup2(fd, 1);     //표준 출력을 fd 가 가리키는 파일로 대체한다.
	close(fd);

	while(cmdvector[idx] != NULL){
		cmdvector[idx] = cmdvector[idx + 2];			//'> ' 다음 내용 정리
		idx++;
	}
	cmdvector[idx] = NULL;

	return 0;
}

int mypipe(char **cmdvector){
	int pipe_cnt = 0;
	int pipe_idx[4];
	/* 첫 번째 파이프 위치 확인 */
	for(int i = 0; cmdvector[i] != NULL; i++){
		if(strcmp(cmdvector[i], "|") == 0){
			pipe_cnt++;
			pipe_idx[pipe_cnt] = i;
		}
	}

	if(pipe_cnt == 0){
		return -1;
	}
	/* cmdvector의 첫 번째 파이프 위치 값을 NULL로 변경 */
	cmdvector[pipe_idx[1]] = NULL;

	/* cmdvector2에 첫 번째 파이프 이후 내용을 옮긴다 */
	int i = pipe_idx[1] + 1;
	int cur = 0;
	for(i = i; cmdvector[i] != NULL && strcmp(cmdvector[i], "|") != 0; i++){
		cmdvector2[cur] = cmdvector[i];
		cur++;
	}
	cmdvector2[cur] = NULL;

	if(pipe_cnt == 2){
		/* cmdvector2의 파이프 이후 내용 cmdvector3에 옮긴다 */
		i = pipe_idx[2] + 1;
		cur = 0;
		for(; cmdvector[i] != NULL; i++){
			cmdvector3[cur] = cmdvector[i];
			cur++;
		}
		cmdvector3[cur] = NULL;
	}

	/* parent process */
	switch(pid=fork()){
	case 0:
		break;
	case -1:
		fatal("(pipe first fork) ");
	default:
		wait(&status);
		if(pipe_cnt < 2){
			return(status);
		}
	}
    
	if(pipe(p) == -1){
		fatal("(pipe call) ");
	}
    
	/* child process */
	switch(pid=fork()){
	case 0:
		if(strchr(cmdline2, '<')){              //redirection 있는지 검사
			redirection_in(cmdvector);
		}
		dup2 (p[1], 1);
		close(p[0]);
		close(p[1]);
		execvp(cmdvector[0], cmdvector);
		fatal("(pipe child) ");
	case -1:
		fatal("(pipe second fork) ");
	default:
		if(pipe_cnt == 1 && strchr(cmdline2, '>')){
			redirection_out(cmdvector);
		}
		dup2 (p[0], 0);
		close(p[0]);
		close(p[1]);
		execvp(cmdvector2[0], cmdvector2);
		fatal("(pipe parent) ");
	}

	/* 파이프가 두 개일 때 */
	if(pipe_cnt == 2){
		if(pipe(p) == -1){
			fatal("(pipe call) ");
		}

		switch(pid=fork()){
		case 0:
			dup2 (p[1], 1);
			close(p[0]);
			close(p[1]);
			fatal("(pipe child) ");
		case -1:
			fatal("(pipe second fork) ");
		default:
			/* redirection 있는지 확인 */
			if(strchr(cmdline2, '>')){
				redirection_out(cmdvector);
			}
			dup2 (p[0], 0);
			close(p[0]);
			close(p[1]);
			execvp(cmdvector3[0], cmdvector3);
			fatal("(pipe parent) ");
		}
	}
	return 0;
}

void execute_cmdline(char* cmdline)
{
    int count = 0;
    int i = 0;

    count = makelist(cmdline, " \t", cmdvector, MAX_CMD_ARG);
    //입력된 명령 없을 시 바로 프롬프트 출력되어 다음 명령어를 받기 위함
    if(count==0) return;
    
//  쉘 종료
    if (strcmp("exit", cmdvector[0]) == 0) {
        exit(1);
    }
//  fork() 전에 디렉토리 변경
    else if (strcmp("cd", cmdvector[0]) == 0) {
        cmd_cd(count, cmdvector);
    }
//	파이프
    else if(strchr(cmdline2, '|')){
        mypipe(cmdvector);
    }
//  백그라운드 프로세스 실행
    else if (strcmp("&", cmdvector[count - 1]) == 0) {
       cmdvector[count -1] = NULL;    //명령어 실행을 위해 '&' 제거
        switch(pid = fork()){
          case 0:
            //redirection 검사
            if(strchr(cmdline2, '<')){
                redirection_in(cmdvector);
            }
            if(strchr(cmdline2, '>')){
                redirection_out(cmdvector);
            }
            execvp(cmdvector[0], cmdvector);
            fatal("main()");
          case -1:
            fatal("main()");
          default:
            //자식 프로세스가 종료되면서 보내는 SIGCHLD 시그널을
            //받은 부모 프로세스는 핸들러 함수 호출.
            signal(SIGCHLD, sigchldhandler);
        }
    }

 
    else {

        switch(pid=fork()){                   //fork 성공 시 부모프로세스는 자식 pid 리턴받음. 자식 프로세스는 0 받음. 실패시 -1리턴
          case 0:   //자식 프로세스가 수행하는 코드
            //redirection 검사
            if(strchr(cmdline2, '<')){
                redirection_in(cmdvector);
            }
            if(strchr(cmdline2, '>')){
                redirection_out(cmdvector);
            }
            execvp(cmdvector[0], cmdvector);
            fatal("main()");

          case -1:    //실패 시 부모 프로세스가 수행하는 코드
              fatal("fork eror");

          default:    //부모 프로세스가 수행하는 코드
              wait(NULL);               //자식 프로세스 종료 기다림.
              fflush(stdout);            //출력 버퍼 비워줘야함.  (안 비우면 에러. 줄바꿈 문자)
        }


    }
}


int main(int argc, char**argv){
  int i=0;

  signal(SIGINT, sighandler);     //제어키에 의해 쉘 종료되지 않도록
  signal(SIGQUIT, sighandler);

  while (1) {

      fputs(prompt, stdout);                 //모니터로 "myshell> "문자열 출력하는 함수 (출력 스트림 : stdout->모니터)
      fgets(cmdline, BUFSIZ, stdin);        //표준 입력(stdin)을 통해 문자열을 입력받아 cmdline배열에 저장
      cmdline[ strlen(cmdline) -1] ='\0';   //널로 문자열 끝을 표시

	 strncpy(cmdline2, cmdline, sizeof(cmdline2));

      execute_cmdline(cmdline);

  }
  return 0;
}
