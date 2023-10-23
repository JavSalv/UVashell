//Autor: Salvador Peñacoba, Javier. 71206212Y

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>

#define ERROR_MSG "An error has occurred\n"
#define PROMPT "UVash> "

//Macro para comprobar punteros null despues de llamar a malloc().
#define NULL_PTR_CHECK(ptr)               \
    if (ptr == NULL){                     \
        fprintf(stderr, "%s", ERROR_MSG); \
        exit(1);                          \
    }

void interactive_loop();
int get_num_args();
char** get_myargv();
void free_commandT();
int builtin_exit();
int builtin_cd();
void remove_tabs();
int correct_redir();
void check_redir();
void comando_unico();
void comandos_paralelos();
int get_num_commands();
void invoke_parallel_commands();
void invoke_command_wrapper();
int check_emptyline();

char delim[2] = " ";
char flecha[2] = ">";
char par[2] = "&";
char* buffer_lectura = NULL;
FILE* batch_file = NULL;

typedef struct{
    char** args;
    int n_args;
    int output_flag;
    char* output_file;
}command_t;

//Cierra (si abiertos) el fichero con comandos y el buffer de lectura por consola.
void funcion_salida(){
    if(batch_file!=NULL)
        fclose(batch_file);
    if(buffer_lectura!=NULL)
        free(buffer_lectura);
}

command_t* parse_command();

int main(int argc, char** argv){
    atexit(funcion_salida);
    if(argc>2){
        fprintf(stderr,"%s",ERROR_MSG);
        exit(1);
    }
    
    if(argc == 2){
        batch_file = fopen(argv[1],"r");
        NULL_PTR_CHECK(batch_file);
        interactive_loop(batch_file);
    }
    else
        interactive_loop(stdin); //modo interactivo
    
    return 0;
}

//Bucle principal del programa.
void interactive_loop(FILE* input){
    
    size_t n = 0;
    buffer_lectura = NULL;  //Cadena leída de consola.
    int char_leidos;        //Caracteres leidos de consola.
    while(1){

        //GET INPUT
        if(input == stdin)
            printf("%s",PROMPT);
        char_leidos = getline(&buffer_lectura,&n,input);
        if(char_leidos == -1) exit(0);
        buffer_lectura[char_leidos-1] = '\0';
        

        //CHECK EMPTY
        if(strlen(buffer_lectura) == 0) continue; //Si introduces cadena vacía salta.
        
        remove_tabs(buffer_lectura);
        
        if(strstr(buffer_lectura,par) == NULL){
            comando_unico(buffer_lectura);
        }
        else
            comandos_paralelos(buffer_lectura);
    }

}

//Un único comando.
void comando_unico(char* buffer){

    if(!correct_redir(buffer)){
        fprintf(stderr,"%s",ERROR_MSG);
        return;
    }

    if(check_emptyline(buffer)){
        return;
    }

    command_t* comando = parse_command(buffer);

    if(comando->output_flag && comando->output_file == NULL){
        free_commandT(comando);
        return;
    }

    //CHECK BUILT-INs
    if(builtin_exit(comando)){}
    else if(builtin_cd(comando)){}

    //EXEC COMMAND
    else{
        command_t** command_wraper = (command_t**)malloc(sizeof(command_t*));
        NULL_PTR_CHECK(command_wraper);
        command_wraper[0] = comando; 
        invoke_command_wrapper(command_wraper,1);
        free(command_wraper);
    }

    free_commandT(comando);
}

//Comandos paralelos con & 
void comandos_paralelos(char* buffer){

    char *save_ptr;
    if(check_emptyline(buffer)){
        return;
    }

    int n = get_num_commands(buffer);
    char* token = __strtok_r(buffer,par,&save_ptr);
    
    if(token == NULL){
        fprintf(stderr,"%s",ERROR_MSG);
        return;
    }
    command_t** comandos = (command_t**)malloc(sizeof(command_t*)*n);
    NULL_PTR_CHECK(comandos);

    int aux = 0;
    
    while(token!=NULL || aux < n){
        if(!correct_redir(token)){
            fprintf(stderr,"%s",ERROR_MSG);
            return;
        }
        comandos[aux] = parse_command(token);
        token = __strtok_r(NULL,par,&save_ptr);
        aux++;
    }

    invoke_command_wrapper(comandos, n);

    for(int i = 0; i<n; i++){
        free_commandT(comandos[i]);
    }
    free(comandos);
}

//Transforma entrada en struct command_t
command_t* parse_command(char* buffer){
    command_t* comando = (command_t*)malloc(sizeof(command_t));
    NULL_PTR_CHECK(comando);
    check_redir(buffer,comando);
    comando->n_args = get_num_args(buffer);
    comando->args = get_myargv(comando->n_args,buffer);
    return comando;
}

//Devuelve 1 si la linea está "vacía", 0 si no lo está.
int check_emptyline(char* buffer){
    int pos = 0;
    while(buffer[pos]){
        if(!isspace(buffer[pos])){
            return 0;
        }
        pos++;
    }
    return 1;
}

//Sustituye tabuladores '\t' por espacios ' '.
void remove_tabs(char* buffer){
    int pos = 0;
    while(buffer[pos]!= '\0'){
        if(buffer[pos] == '\t') buffer[pos] = ' ';
        pos++;
    }
}

//Devuelve el numero de argumentos (argc) del comando.
int get_num_args(char* input){
    char * save_ptr;
    char *input_copy = strdup(input);

    NULL_PTR_CHECK(input_copy);

    char *token = __strtok_r(input_copy,delim,&save_ptr);
    int n_args = 0;
    while(token!=NULL){
        n_args++;
        token = __strtok_r(NULL,delim,&save_ptr);
    }
    free(input_copy);
    return n_args;
}

//Devuelve el argv del comando.
char** get_myargv(int my_argc,char* buffer){
    char* save_ptr;
    char** my_argv = (char**)malloc((my_argc+1) * sizeof(char*));
    NULL_PTR_CHECK(my_argv);
    char* token = __strtok_r(buffer, delim,&save_ptr);
    int aux = 0;
    while(token != NULL && aux< my_argc){
        my_argv[aux] = strdup(token);
        NULL_PTR_CHECK(my_argv[aux]);
        aux++;
        token = __strtok_r(NULL,delim,&save_ptr);
    }
    my_argv[my_argc] = NULL;

    return my_argv;
}

//Libera el struct command_t. Libera todos sus campos y luego el propio struct.
void free_commandT(command_t* comando){
    for(int i = 0; i < comando->n_args; i++){
        free(comando->args[i]);
    }
    free(comando->args);
    if(comando->output_flag && comando->output_file != NULL) free(comando->output_file);
    free(comando);
}

//Comando built-in cd.
int builtin_cd(command_t* comando){
    if(strcmp(comando->args[0],"cd") != 0){
        return(0);
    }
    if(comando->n_args>2 || chdir(comando->args[1]) == -1){
        fprintf(stderr,"%s",ERROR_MSG);
        return(1);
    }
    return(1);
}

//Comando built-in exit.
int builtin_exit(command_t* comando){
    if(strcmp(comando->args[0],"exit")!=0){
        return(0);
    }
    if(comando->n_args > 1){
        fprintf(stderr,"%s",ERROR_MSG);
        return(1);
    }
    free_commandT(comando);
    exit(0);
}

//Ejecuta el comando pasado como parámetro
void invoke_command(command_t* comando){

    if(comando->output_flag){  
        int fd = open(comando->output_file, O_WRONLY | O_CREAT | O_TRUNC ,S_IRWXG|S_IRWXO|S_IRWXU);
        dup2(fd,1);
        dup2(fd,2);
    }
    if(execvp(comando->args[0],comando->args) == -1){
        fprintf(stderr,"%s",ERROR_MSG);
    }
}

//Wrapper para la función invoke_command. Genera n procesos hijos que ejecutan la funcion invoke_command.
void invoke_command_wrapper(command_t** lista_comandos, int num_comandos){
    for(int i = 0; i< num_comandos; i++){
        pid_t child = fork();

        if(child == 0){
            invoke_command(lista_comandos[i]);
            exit(0);
        }
    }

    for(int i = 0; i<num_comandos; i++){
        wait(0);
    }
}

//Devuelve 0 si la sintaxis de la redireccion es incorrecta, 1 si redireccion correcta.
int correct_redir(char* buffer){
    //Comprueba que solo haya un simbolo de redir por comando

    int pos = 0;
    int n = 0;
    int ret = 0;
    while(buffer[pos] != '\0'){
        if(buffer[pos] == '>'){
            if(++n>1)
            return ret;
        }
        pos++;
    }
    //Comprueba que hay un unico argumento de redireccion
    char* buffer_copia = strdup(buffer);
    NULL_PTR_CHECK(buffer_copia);
    strtok(buffer_copia, flecha);
    char* token = strtok(NULL, flecha);
    strtok(token, delim);
    if(strtok(NULL,delim)==NULL){
        ret = 1;
    }
    free(buffer_copia);
    return ret;
}

//Ajusta los campos de redireccion de command_t.
void check_redir(char* buffer, command_t* comando){
    if(strstr(buffer,flecha) == NULL){
        comando->output_flag = 0;
        return;
    }
    buffer = strtok(buffer, flecha);
    char* filename = strtok(NULL,flecha);
    comando->output_flag = 1;
    if(filename == NULL){
        fprintf(stderr,"%s",ERROR_MSG);
        return;
    }
    comando->output_file = strdup(strtok(filename,delim));
    NULL_PTR_CHECK(comando->output_file);
}

//Devuelve el número de comandos paralelos en la misma linea.
int get_num_commands(char* buffer){
    char* buffer_copia = strdup(buffer);
    NULL_PTR_CHECK(buffer_copia);
    int num = 0;
    char* token = strtok(buffer_copia,par);
    while(token != NULL){
        num++;
        token = strtok(NULL,par);
    }
    free(buffer_copia);
    return num;
}

