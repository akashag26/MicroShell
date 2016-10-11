/******************************************************************************
 *
 *  File Name........: main.c
 *
 *  Description......: Micro Shell 
 *
 *  Author...........: Akash Agrawal 
 *
 *****************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "parse.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/resource.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <termios.h>
#define handle_error(msg) \
	do { perror(msg); exit(0); } while (0)
/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */
#define MAXJOBS      16 


int  pipe1[2] , pipe2[2];
void executeRedirection(Cmd c);
int numberofchildren=0;
void executeCommand(Cmd c);
int isPipe1, isPipe2;
void sig_handler(int signo);
pid_t mainpid;
char *str= '\0';
extern char** environ;
int mainErr, mainInput, mainOutput;
char *sysPath;
char *sysHome;
char *builtin[] = {"cd","where","nice","unsetenv","setenv","pwd","echo","logout"}; 

int nextjid = 1 ;
struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char **args;  /* command line */
};


/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    //args=NULL;
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}


static int isBuiltin(char *c){
	int i;         	
	for(i=0;i<8;i++)
	{
		if(strcmp(c, builtin[i])==0)
		{
			return 1;
		}
	}
	return 0;
}

static void prCmd(Cmd c)
{
	int i;
	if ( c ) {
		if(!c->next){
			if(isBuiltin(c->args[0])==1)
			{
				executeCommand(c);
				return;
			}
		}
		// this driver understands one crsommand
		if ( !strcmp(c->args[0], "end") )
			exit(0);

		executeRedirection(c);


	}
}

void executeCommand(Cmd c){
	char currentDirectory[1024];
	int i;
	int inputFile,outputFile; 

	pid_t niceid;

	if(c->in==Tin)
	{       
		//	printf("Ibput take from file"); 
		if((inputFile = open(c->infile, O_RDONLY))<0)
			handle_error("\n File Error: Open Failed");	
		else
		{       
			if(dup2(inputFile, 0) <0)
			{       
				handle_error("Dup2 Failed");
			}
			close(inputFile);
		}

	}

	if(c->out!=Tnil)
	{

		if(c->out == Tout)
		{
			if((outputFile = open(c->outfile,O_CREAT|O_WRONLY|O_TRUNC,0666))<0)
				handle_error("\n File Error: Open Failed");
			else
			{       
				if(dup2(outputFile, 1) <0)
				{       
					handle_error("Dup2 Failed");
				}
				close(outputFile);
			}
		}




		if(c->out == Tapp)
		{
			if((outputFile = open(c->outfile,O_CREAT|O_WRONLY|O_APPEND,0666))<0)
				handle_error("\n File Error: Open Failed");
			else
			{
				if(dup2(outputFile, 1) <0)
				{
					handle_error("Dup2 Failed");
				}
				close(outputFile);
			}
		}  

		if(c->out == ToutErr)
		{
			if((outputFile = open(c->outfile,O_CREAT|O_WRONLY|O_TRUNC,0666))<0)
				handle_error("\n File Error: Open Failed");
			else
			{
				if(dup2(outputFile, 1) <0)
				{
					handle_error("Dup2 Failed");
				}

				if(dup2(outputFile,2) <0)
				{
					handle_error("Dup2 Failed");
				}
				close(outputFile);
			}
		}

		if(c->out == TappErr)
		{
			if((outputFile = open(c->outfile,O_CREAT|O_WRONLY|O_APPEND,0666))<0)
				handle_error("\n File Error: Open Failed");
			else
			{
				if(dup2(outputFile, 1) <0)
				{
					handle_error("Dup2 Failed");
				}

				if(dup2(outputFile, 2) <0)
				{
					handle_error("Dup2 Failed");
				}
				close(outputFile);
			}
		}

	}
	//printf("Checks Done");
	if(strcmp(c->args[0], "pwd")==0) {
		if(getcwd(currentDirectory, sizeof(currentDirectory))!=NULL)
			printf("%s\n",currentDirectory);
	}

	if(strcmp(c->args[0], "logout")==0)
		exit(0);

	if(strcmp(c->args[0], "echo")==0){
		for( i = 1; c->args[i]!=NULL ; i++){
			if(c->args[i+1]==NULL)
				printf("%s", c->args[i]);
			else	
				printf("%s ", c->args[i]);
		}
		printf("\n");
	}

	if(strcmp(c->args[0],"cd")==0){
		if(!c->args[1]){
			//printf("Hello");
			if(chdir(getenv("HOME"))==-1){
				printf("cd: %s No such Home directory\n", getenv("HOME"));
			}       

		}
		else
		{
			if(chdir(c->args[1])==-1){
				printf("cd: %s: No such file or directory\n",c->args[1]);
			}
		}

	}

	if(strcmp(c->args[0],"setenv")==0){
		if(c->args[1]==NULL){
			for(i=0; environ[i]!=NULL; i++)
				printf("%s \n", environ[i]);
		}
		else 
			if(c->args[2]!=NULL)
			{	
				if(setenv(c->args[1], c->args[2],1)==-1)
					printf("Error with Setenv");
			}
			else
			{
				if(setenv(c->args[1], NULL,1)==-1)
					printf("Error with Setenv");	
			}
	}

	if(strcmp(c->args[0],"unsetenv")==0){
		if(c->args[1]!=NULL)
		{
			if(unsetenv(c->args[1])==-1)
				printf("Error in UnsetEnv");
		}
	}


	if(strcmp(c->args[0], "where")==0){
		char path[1024],command[1024];
		char *pth;
		int i=0;
		for(i=1;c->args[i]!=NULL; i++){
			if(isBuiltin(c->args[i])==1)
				printf("%s is a shell built-in\n", c->args[i]);
		
			
				strcpy(path,sysPath);
				pth = strtok(path, ":");
				while(pth!=NULL)
				{
					strcpy(command, pth);
					strcat(command, "/");
					strcat(command, c->args[i]);
					if(!access(command, X_OK))
						printf("%s\n", command);
					pth = strtok(NULL,":");
				}
		

		}
	}


	if(strcmp(c->args[0], "nice")==0){
		int isNum=1;
		int val=4;
		if(c->args[1]!=NULL)
		{
			val = atoi(c->args[1]);
			char *abc = c->args[1];
			int j;
			for(j=0; j<strlen(c->args[1]); j++)
			{
				if(j==0){
					if((abc[0]=='-') || (abc[0]=='+')) 
					{
						if(strlen(c->args[1])>1)
						{
							continue;
						}
						else
						{
							isNum=0;
							val=4;
							break;
						}
					}			

				}

				if(!isdigit(abc[j])){
					isNum=0;
					val =4; 
					break;			
				}							
			}
		}
		int count = 0;
		if(c->nargs==1){
			if(setpriority(PRIO_PROCESS, 0, val)==-1)
			{
				perror("Nice can't be Set");	
				return;
			}
			count =2;	
			printf("%d \n",getpriority(PRIO_PROCESS, 0));
		}
		else
			if(c->nargs>=2){
				if(isNum==1 && c->nargs==2)	
				{	
					if(setpriority(PRIO_PROCESS, 0, val)==-1)	
					{
						perror("Nice can't be Set");
						return;
					}		
					printf("%d\n",getpriority(PRIO_PROCESS, 0));
					count =2;
				}
				else
				{ if(c->nargs>2 && isNum==1){
								    if(isBuiltin(c->args[2])==1){

									    if(setpriority(PRIO_PROCESS, 0, val)==-1)
									    {
										    perror("Nice can't be Set");
										    return;
									    }
									    Cmd newC= malloc(sizeof(*newC));
									    if(newC==NULL)
										    handle_error("Memory");

									    int k=2;
									    for( ;c->args[k]!=NULL ; )
									    {       
										    k++;
									    }
									    newC->args = malloc(sizeof(char*)*(k-1));
									    if(newC->args==NULL)
										    handle_error("Memory");
									    newC->in =Tnil;
									    newC->out = Tnil;
									    newC->next= NULL;
									    newC->nargs=k-2;
									    newC->maxargs= k-2;
									    int l=0;
									    count =k;
									    for(k=2; c->args[k]!=NULL ; k++,l++)
									    {       
										    int size = strlen(c->args[k]);
										    newC->args[l] = malloc(sizeof(char)*size);
										    if(newC->args[l] ==NULL)
											    handle_error("Memory Allocation Error");
										    if(strcpy(newC->args[l],c->args[k])==NULL)
											    handle_error("String Copy Error");
									    }    
									    executeCommand(newC);
									    free(newC->args);
									    free(newC);

								    }
							    }
								    else
								    {

									    if(isBuiltin(c->args[1])==1){
										    if(setpriority(PRIO_PROCESS, 0, val)==-1)
										    {
											    perror("Nice 3 Value can't be Set");
											    return;    
										    }
										    Cmd newC= malloc(sizeof(*newC));
										    if(newC==NULL)
											    handle_error("Memory");

										    int k=1;
										    for( ;c->args[k]!=NULL ; )
										    {
											    k++;
										    }
										    newC->args = malloc(sizeof(char*)*(k));
										    if(newC->args==NULL)
											    handle_error("Memory");
										    newC->in =Tnil;
										    newC->out = Tnil;
										    newC->next= NULL;
										    newC->nargs=k-1;
										    int l=0;
										    count =k;
										    for(k=1; c->args[k]!=NULL ; k++,l++)	
										    {
											    int size = strlen(c->args[k]);
											    newC->args[l] = malloc(sizeof(char)*size);
											    if(newC->args[l] ==NULL)
												    handle_error("Memory Allocation Error");	
											    if(strcpy(newC->args[l],c->args[k])==NULL)
												    handle_error("String Copy Error");
										    }
										    executeCommand(newC);
										    free(newC->args);
										    free(newC);    
									    }
								    }

								    if(count ==0){
									    int status;    
									    niceid= fork();
									    numberofchildren++;   
									    if(niceid<0)
										    handle_error("Cannot Fork");

									    else if(niceid==0){
										    if(setpriority(PRIO_PROCESS, 0, val)==-1)
										    {
											    handle_error("Nice can't be Set");
											    return;
										    }	
										    if(c->nargs>2 && isNum==1)
										    {	if(execvp(c->args[2],&c->args[2])==-1)
											 {
												if(errno==ENOENT)
                                									printf ("Command not found \n");
                                								else
                                								if((errno==EACCES)|| (errno==EISDIR))
                                									printf("Permission Denied \n");
                                								else
                                									handle_error(strerror(errno));
											}	
										    }
										    else{
											    if(execvp(c->args[1], &c->args[1])==-1)
												{
												if(errno==ENOENT)
                                									printf ("Command not found \n");
                                								else
                                								if((errno==EACCES)|| (errno==EISDIR))
                                									printf("Permission Denied \n");
                                                                                                else
                                                                                                        handle_error(strerror(errno));
												}  
										    }
										    exit(EXIT_FAILURE);

									    }
									    else
									    {
										   // wait()
										   waitpid(-1, NULL, WUNTRACED);
									    }
								    }	

				}
			}
	}
	dup2(mainOutput,1);
	dup2(mainInput,0);
	dup2(mainErr,2);

}

void executeRedirection(Cmd c){
	int inputFile,outputFile;
	pid_t pid, wpid;
	int status;
	int found;
	pid = fork();
	numberofchildren++;
	if(pid == 0)
	{       
		// "<"
		if(c->in==Tin)
		{
			if((inputFile = open(c->infile, O_RDONLY))<0)
				handle_error("\n File Error: Open Failed");
			else
			{
				if(dup2(inputFile, 0) <0)
				{
					handle_error("Dup2 Failed");
				}
				close(inputFile);
			}

		}

		// ">"
		if(c->out == Tout)
		{	
			if((outputFile = open(c->outfile,O_CREAT|O_WRONLY|O_TRUNC,0666))<0)
				handle_error("\n File Error: Open Failed");
			else
			{
				if(dup2(outputFile, 1) <0)
				{
					handle_error("Dup2 Failed");
				}
				close(outputFile);
			}
		}	

		// ">>"
		if(c->out == Tapp)
		{
			if((outputFile = open(c->outfile,O_CREAT|O_WRONLY|O_APPEND,0666))<0)
				handle_error(strerror(errno));
			else
			{
				if(dup2(outputFile, 1) <0)
				{
					handle_error("Dup2 Failed");
				}
				close(outputFile);
			}
		}                                                                             

		// ">&"
		if(c->out == ToutErr)
		{
			if((outputFile = open(c->outfile,O_CREAT|O_WRONLY|O_TRUNC,0666))<0)
				handle_error("\n File Error: Open Failed");
			else
			{
				if(dup2(outputFile, 1) <0)
				{
					handle_error("Dup2 Failed");
				}

				if(dup2(outputFile, 2) <0)
				{
					handle_error("Dup2 Failed");
				}
				close(outputFile); 
			}
		}

		// ">>&"
		if(c->out == TappErr)
		{
			if((outputFile = open(c->outfile,O_CREAT|O_WRONLY|O_APPEND,0666))<0)
				handle_error("\n File Error: Open Failed");
			else
			{
				if(dup2(outputFile, 1) <0)
				{
					handle_error("Dup2 Failed");
				}

				if(dup2(outputFile, 2) <0)
				{
					handle_error("Dup2 Failed");
				}
				close(outputFile); 
			}
		}

		if(c->in==Tpipe || c->in == TpipeErr)
		{
			if(isPipe1==0)
			{
				close(pipe1[1]);
				dup2(pipe1[0], 0);
				close(pipe1[0]);
			}
			else
				if(isPipe2==0)
				{
					close(pipe2[1]);
					dup2(pipe2[0],0);
					close(pipe2[0]);
				}
		}

		if(c->next!=NULL && (c->out ==Tpipe || c->out == TpipeErr))
		{

			if(isPipe1==1)
			{
				close(pipe1[0]);
				dup2(pipe1[1],1);
				close(pipe1[1]);
			}
			else
				if(isPipe2==1)
				{
					close(pipe2[0]);
					dup2(pipe2[1],1);
					close(pipe2[1]);
				}

			if(c->out ==TpipeErr)
			{
				dup2(1, 2);
			}
		}

		if(inputFile>0)
		{
			close(inputFile);
		}

		if(outputFile>0)
		{
			close(outputFile);
		}


		if(isBuiltin(c->args[0])==1){
			executeCommand(c);
			exit(0);
		}
		else
			if ( execvp(c->args[0], c->args) == -1)
			{
		
				if(errno==ENOENT)	
				printf ("Command not found \n");
				else
				if((errno==EACCES)|| (errno==EISDIR))
				printf("Permission Denied \n");
				else
				handle_error(strerror(errno));
			}	
		exit(EXIT_FAILURE);

	}
	else if (pid < 0)
	{
		handle_error("ush");
	}
	else
	{

		if(isPipe1==1 && isPipe2==0)
		{
			close(pipe1[1]);
			isPipe1=0;
			isPipe2=1;
		}
		else
			if(isPipe1==0 && isPipe2==1)
			{
				close(pipe2[1]);
				isPipe1=1;
				isPipe2=0;
			}

		if(isPipe1==1)
			pipe(pipe1);
		else
			if(isPipe2==1)
				pipe(pipe2); 
		//wait();
	}


}
static void prPipe(Pipe p)
{
	int i = 0;
	Cmd c;

	//	fflush(stdin);
	//	fflush(stdout);

	int isPipe = 0;
	int countPipe = 0;


	if ( p == NULL )
		return;

	isPipe1=0;
	isPipe2=1;
	if(pipe(pipe1)==-1) perror("Pipe");
	if(pipe(pipe2)==-1) perror("Pipe");

	for ( c = p->head; c != NULL; c = c->next ) {
		prCmd(c);
	}		

	int j;
	for(j=0; j<numberofchildren ; j++)
		waitpid(-1, NULL, WUNTRACED);
	numberofchildren = 0;
	prPipe(p->next);
}

void sig_handler(int signo){
	if(getpid()!=mainpid)
		exit(0);
}

int main(int argc, char *argv[])
{
	Pipe p;
	char *host = "armadillo"; 
	mainOutput = dup(STDOUT_FILENO);
	mainInput = dup(STDIN_FILENO);
	mainErr = dup(STDERR_FILENO);
	int backup = dup (STDIN_FILENO);
	mainpid = getpid();
	signal(SIGTERM, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);	
	if(signal(SIGINT, sig_handler)==SIG_ERR)
		perror("Can't Catch SIGINT \n");	
	if(signal(SIGQUIT, sig_handler)==SIG_ERR)
		perror("Can't Catch SIGINT \n");
	if(signal(SIGTERM, sig_handler)==SIG_ERR)
		perror("Can't Catch SIGINT \n");	
	if(signal(SIGTSTP, sig_handler)==SIG_ERR)
                perror("Can't Catch SIGINT \n");


	sysPath = getenv("PATH");
	sysHome = getenv("HOME");
	int fin;

	char ushrc[1024];

	strcpy(ushrc, sysHome);
	strcat(ushrc,"/.ushrc");
	int ushrcFile;
	if((ushrcFile = open(ushrc, O_RDONLY))<0)
		printf("\n File Error: Open Failed");
	else
	{
		if(dup2(ushrcFile, 0) <0)
		{
			handle_error("Dup2 Failed");
		}
		close(ushrcFile);
		p = parse();	
		while(p!=NULL)
		{
			prPipe(p);
			freePipe(p);	
			wait(NULL);
			p = parse();
		}
		fflush(stdin);

		if(dup2(mainInput,0) <0)
		{
			handle_error("Dup2 Failed");
		}
		fflush(stdin);
	}

	setbuf(stdin,NULL);
	setbuf(stdout,NULL);
	setbuf(stderr,NULL);

	while ( 1 ) {
		fflush(stdout);
			
			char hostname[1024];
			hostname[1023] = '\0';
			gethostname(hostname, 1023);
		printf("%s%% ", hostname);
		p = parse();
		prPipe(p);    
		freePipe(p);
	}
	close(mainInput);
	close(mainOutput);
	close(mainErr);

}
/*........................ end of main.c ....................................*/
