#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "list.h"

#define MAX_CMD_LEN 512
#define MAX_LIST_LEN 10

struct command_node{
    
    struct list_elem elem;
    char command[MAX_CMD_LEN];
    int command_number;
};

struct list command_list;
int num_commands = 0;

void read_line(char *line){

    char buf[1];
    int i = 0;
   
    while(read(0, buf, 1) > 0){
        if(buf[0] == '\n'){
            break;
        }

        line[i] = buf[0];
        i = i + 1;

        if(i > (MAX_CMD_LEN - 1)){
            break;
        }
    }
    line[i] = '\0';

    return;
}

int count_digits(int x){
    
    if(x < 10) return 1;
    if(x < 100) return 2;
    if(x < 1000) return 3;
    if(x < 10000) return 4;
    if(x < 100000) return 5;
    if(x < 1000000) return 6;
    return 7; 
}

void print_prompt(){

    char str[4 + count_digits(num_commands)]; 
    sprintf(str, "[%d]$ ", num_commands);
    write(1, str, strlen(str));
}

void parse_args(char *cmd, char *arr[]){

    char *token;

    token = strtok(cmd, " ");
    int i = 0;

    while(token != NULL){
        arr[i] = token;
        token = strtok(NULL, " ");    
        i++;
    }
    arr[i] = NULL;
}

void parse_args_until_token(char *cmd, char *arr[], char *element){
    
    char *token;

    token = strtok(cmd, " ");
    int i = 0;

    while(strcmp(token, element) != 0){
        
        arr[i] = token;
        token = strtok(NULL, " ");
        i++;
    }
    arr[i] = NULL;
}

char* find_redirect_file(char *cmd){
    
    char *token;
    char *temp = strstr(cmd, ">");
    
    token = strtok(temp, " ");
    token = strtok(NULL, " ");

    return token;
}

void exec_redirect(char *cmd){
    
    pid_t id;
    int fd;
    int exit_code;

    char *file_name = find_redirect_file(cmd);
    
    if((fd = open(file_name, O_CREAT | O_WRONLY | O_TRUNC, 0644)) < 0){
        printf("Unable to open the file: %s.\n", file_name);
        exit(1);
    }
    
    id = fork();

    if(id == 0){
        char *cmd_args[512];
        close(1);
        dup(fd);
        close(fd);
        parse_args_until_token(cmd, cmd_args, ">");
        execvp(cmd_args[0], cmd_args);
        printf("usfsh: command not found: %s\n", cmd);
        exit(-1);
    }
    else{
        close(fd);
        id = wait(&exit_code);
    }
}

void exec_pipe(char *cmd){
    
    int pipes[2];
    pid_t id;

    char *right_side_cmd[512];
    char *left_side_cmd[512];
    
    pipe(pipes);
       
    parse_args_until_token(cmd, right_side_cmd, "|");
    char *token = strtok(strstr(cmd, "|"), "|");
    parse_args(token, left_side_cmd);

    id = fork();
     
    if(id == 0){
                
        close(0);
        dup(pipes[0]);
        close(pipes[1]);
        close(pipes[0]);
         
        execvp(left_side_cmd[0], left_side_cmd);
        printf("usfsh: command not found: %s\n", token);
        exit(1);
    }
    
    id = fork();

    if(id == 0){
        
        close(1);
        dup(pipes[1]);
        close(pipes[0]);
        close(pipes[1]);

        execvp(right_side_cmd[0], right_side_cmd);
        printf("usfsh: command not found.\n");
        exit(1);

    }
    
    close(pipes[0]);
    close(pipes[1]);

    id = wait(NULL);
    id = wait(NULL);
}

void exec_command(char *cmd){

    pid_t id;
    int exit_code;

    if(strstr(cmd, ">") != NULL){
        exec_redirect(cmd);
    }
    else if(strstr(cmd, "|") != NULL){
        exec_pipe(cmd);
    }
    else{
        id = fork();

        if (id == 0) {
            /* we are in the child */
            char *cmd_args[512];
            parse_args(cmd, cmd_args);
            execvp(cmd_args[0], cmd_args);
            printf("sish: command not found: %s\n", cmd);
            exit(-1);
        } else {
            /* we are in the parent */
            id = wait(&exit_code);
        }
    }
}

bool isExit(char line[]){

    if(strlen(line) >= 4 && memcmp(line, "exit", 5) == 0 || strlen(line) >= 5 && memcmp(line, "exit ", 5) == 0){
        return true;
    }
    return false;
}

bool isHistory(char line[]){

    if(strlen(line) >= 7 && memcmp(line, "history", 8) == 0 || strlen(line) >= 8 && memcmp(line, "history ", 8) == 0){
        return true;
    }


    return false;
}

bool isChangeDirectory(char line[]){

    if(strlen(line) >= 2 && memcmp(line, "cd", 3) == 0 || strlen(line) >= 3 && memcmp(line, "cd ", 3) == 0){
        return true;
    }
    return false;
}

void print_command_list(){

    struct list_elem *e;
     
    printf("History:\n");

    for(e = list_begin(&command_list); e != list_end(&command_list); e = list_next(e)){
        struct command_node *n = list_entry(e, struct command_node, elem);
        printf("%d  %s \n", n->command_number, n->command);
    }
}

struct command_node* create_new_node(int command_num, char com[]){

    struct command_node *node;

    node = (struct command_node*) malloc(sizeof(struct command_node));

    if(node == NULL){
        printf("Problem malloc'ing. Now exiting\n");
        exit(-1);
    }
    
    node->command_number = command_num;
    strcpy(node->command, com);
    return node;
}

bool isDigit(char character){
    
    if(character >= '0' && character <= '9'){
        return true;
    }
    return false;
}


void get_correct_command_digit(char prefix[], char command[]){
    
    /*This is the integer version*/
    int digit = atoi(prefix);

    struct list_elem *e;

    for(e = list_rbegin(&command_list); e != list_rend(&command_list); e = list_prev(e)){
        
        struct command_node *node = list_entry(e, struct command_node, elem);
        if(node->command_number == digit){
            strcpy(command, node->command);
            return;
        }
    }

    printf("No command was found\n");
    exit(-1);
}

void get_correct_command(char prefix[], char command[]){
    
    /*String version*/
    struct list_elem *e;
    
    for(e = list_rbegin(&command_list); e != list_rend(&command_list); e = list_prev(e)){
        struct command_node *node = list_entry(e, struct command_node, elem);
        if(strncmp(node->command, prefix, strlen(prefix)) == 0){
            strcpy(command, node->command);
            return;
        }
    }

    printf("No command was found\n");
    exit(-1);
}

void isBang(char command[]){

    if(command[0] != '!'){
        return;
    }

    if(isDigit(command[1])){
        char prefix[MAX_CMD_LEN];
        strcpy(prefix, command+1);
        get_correct_command_digit(prefix, command);
    }
    else{
        char prefix[MAX_CMD_LEN];
        strcpy(prefix, command+1);
        get_correct_command(prefix, command); 
    }
}

bool process_one_command(){

    bool done = false;
    char buf[MAX_CMD_LEN];

    struct command_node *node;
    print_prompt();
    read_line(buf);

    if(strlen(buf) == 0){
        return done;
    }
    
    num_commands++;

    isBang(buf);

    if(buf[0] != '!'){ 
        node = create_new_node(num_commands, buf);
    }

    if(list_size(&command_list) > MAX_LIST_LEN){
       free(list_pop_front(&command_list));
    }
     
    list_push_back(&command_list, &node->elem);
    
    if(isExit(buf)){
        done = true;
    }
    else if(isChangeDirectory(buf)){
        char* cd = strtok(buf, " ");
        
        cd = strtok(NULL, " ");
        
        if(cd == NULL){
            chdir(getenv("HOME"));
        }
        else{
            chdir(cd);
        }
    }
    else if(isHistory(buf)){
        print_command_list();
    }
    else{
        exec_command(buf);
    }
    
    return done;
}

int main(int argc, char **argv){

    bool done = false;
    list_init(&command_list);

    while(!done){
        done = process_one_command(num_commands);
    }
    
    return 0;
}

void debug_panic (const char *file, int line, const char *function,
                  const char *message, ...){

      va_list args;
      printf ("Kernel PANIC at %s:%d in %s(): ", file, line, function);

      va_start (args, message);
      vprintf (message, args);
      printf ("\n");
      va_end (args);
      exit(-1);
}
