//
//  Trabalho Pratico 1
//  Sistemas Operativos 2013/2014
//
//  Grupo 2 (Turma 2MIEIC03)
//

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>

#include <signal.h>

#include <libgen.h>
#include <time.h>

#define DEBUG 1

//  Global Variables

const char *tail_path = "/usr/bin/tail";
const char *grep_path = "/usr/bin/grep";

char *files[32];

pid_t procv[32];
int procc;

pid_t _monitor_pid;

#pragma mark grep Thread

pid_t _pid_to_kill;

void alarm_handler(__attribute__((unused)) int signo) {
    if (_pid_to_kill) {
        
#if DEBUG
        
        printf("alarm_handler: Got alarm, sending kill signal to %d...\n", _pid_to_kill);
        
#endif
        
        int k = _pid_to_kill;
        
        _pid_to_kill = 0;
        
        kill(k, SIGKILL);
    }
}

void check_match_grep_term(__attribute__((unused)) int signo) {
    //  Clean up check_match_grep.
    
#if DEBUG
    
    printf("check_match_grep_term: Sending kill signal to %d...\n", _pid_to_kill);
    
#endif
    
    kill(_pid_to_kill, SIGTERM);
    
    exit(0);
}

bool check_match_grep(const char *filename, const char *line, const char *match) {
    signal(SIGTERM, &check_match_grep_term);
    
    pid_t pid;
    
    int pipe1[2], pipe2[2];
    
    pipe(pipe1);
    pipe(pipe2);
    
    if (( pid = fork() )) {
        //  Parent Process
        
        _pid_to_kill = pid;
        
        alarm(1);
        
        signal(SIGALRM, &alarm_handler);
        
        //  Now...
        
        char buffer[1024];
        
        close(pipe1[0]);
        close(pipe2[1]);
        
        if ( write(pipe1[1], line, strlen(line) ) != (long) strlen(line) )
            return false;
        
        read(pipe2[0], buffer, sizeof(buffer));
        
        if (!_pid_to_kill)
            return false;
        
        _pid_to_kill = 0;
        
        if (strlen(buffer) > 0) {
            time_t t = time(NULL);
            struct tm * p = localtime(&t);
            
            char s[1024];
            
            strftime(s, 1000, "%FT%T", p);
            
            strcat(s, " - ");
            strcat(s, filename);
            strcat(s, " - ");
            strcat(s, line);
            
            printf("%s", s);
        }
    } else {
        // Child Process
        
        close(pipe1[1]);
        close(pipe2[0]);
        
        if (pipe1[0] != STDIN_FILENO) {
            if (dup2(pipe1[0], STDIN_FILENO) != STDIN_FILENO) {
              
#if DEBUG
              	
                printf("Pipe Creation Error 1");
              	
#endif
                
                return false;
            }
            
            close(pipe1[0]);
        }
        
        if (pipe2[1] != STDOUT_FILENO) {
            if (dup2(pipe2[1], STDOUT_FILENO) != STDOUT_FILENO) {
              
#if DEBUG
              	
                printf("Pipe Creation Error 2");
              	
#endif
                
                return false;
            }
            
            close(pipe2[1]);
        }
        
        execlp(grep_path, "grep", "--line-buffered", match, (char *) 0);
    }
    
    return false;
}

#pragma mark -
#pragma mark File Watcher Thread

pid_t child_tail;
pid_t child_grep_thread;

void watch_file_term(__attribute__((unused)) int signo) {
    //  Clean up watch_file.
    
#if DEBUG
    
    printf("watch_file_term: Sending kill signals to %d and %d...", child_tail, child_grep_thread);
    
#endif
    
    kill(child_tail, SIGTERM);
    kill(child_grep_thread, SIGTERM);
}

void watch_file(const char *string, const char *filepath) {
    signal(SIGTERM, &watch_file_term);
    signal(SIGUSR1, &watch_file_term);
    
    pid_t pid;
    
    int pipefd[2];
    pipe(pipefd);
    
    if (( pid = fork() )) {
        //  Parent Process
        
        child_tail = pid;
        
        char buffer[1024];
        
        close(pipefd[1]);
        
        while (read(pipefd[0], buffer, sizeof(buffer)) != 0) {
            pid_t pid2;
            
            if ( !(pid2 = fork()) ) { //  Let's not block the main process.
                char bn[512];
                strcpy(bn, filepath);
                
                check_match_grep(basename(bn), buffer, string);
                
                exit(0);
            }
            
            child_grep_thread = pid2;
        }
    } else {
        //  Child Process
        
        close(pipefd[0]);
        
        dup2(pipefd[1], 1);
        dup2(pipefd[1], 2);
        
        close(pipefd[1]);
        
        execlp(tail_path, "tail", "-f", "-n", "0", filepath, (char *) 0);
    }
}

#pragma mark -
#pragma mark Monitor Thread

void monitor() {
    int rem = 0;
    
    for (int i = 0; i < procc; i++) {
        if (!strcmp(files[i], "REM")) {
            rem++;
            
            continue;
        }
        
        if (access(files[i], F_OK) == -1) {
            files[i] = malloc(strlen("REM") + 1);
            
            strcpy(files[i], "REM");
            
#if DEBUG
            
            printf("monitor: Killing child process %d...\n", procv[i]);
            
#endif
            
            kill(procv[i], SIGUSR1);
        }
    }
    
    if (rem == procc) {
        //  All files have been removed.
        
#if DEBUG
        
        printf("monitor: Killing parent process %d...\n", getppid());
        
#endif
        
        kill(getppid(), SIGUSR2);
        
        exit(0);
    }
    
    sleep(5);
    
    monitor();
}

#pragma mark -
#pragma mark Main Thread

void clean_up(__attribute__((unused)) int signo) {
    //  Clean everything up!
    
    printf("clean_up: Cleaning up and terminating...\n");
    
    kill(_monitor_pid, SIGTERM);
    
    for (int i = 0; i < procc; i++)
        kill(procv[i], SIGTERM);
    
    exit(0);
}

int main(int argc, const char *argv[]) {
    if (argc < 4) {
        char progname[512];
        strcpy(progname, argv[0]);
        
        printf("Usage: %s time string file1 [file2 [...]]\n", basename(progname));
        
        return 0;
    }
    
    for (int i = 3; i < argc; i++) {
        pid_t forked_pid;
        
        if (( forked_pid = fork() )) {
            //  Parent Process
            
            procv[i - 3] = forked_pid;
            
            files[i - 3] = malloc(strlen(argv[i] + 1));
            strcpy(files[i - 3], argv[i]);
            
            procc++;
        } else {
            // Child Process
            
            watch_file(argv[2], argv[i]);
            
            return 0;
        }
    }
    
    signal(SIGUSR2, &clean_up);
    
    if (( _monitor_pid = fork() )) {
        monitor();
        
        return 0;
    }
    
    sleep(atoi(argv[1]));
    
    clean_up(0);
    
    return 0;
}
