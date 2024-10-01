#include <stdio.h>      // Para funciones de entrada/salida
#include <stdlib.h>     // Para funciones de manejo de memoria
#include <pthread.h>    // Para funciones de manejo de hilos y mutex
#include <time.h>       // Para medir el tiempo

// Definición de la estructura para los nodos de una lista enlazada
struct list_node_s {
    int data;
    struct list_node_s* next;
    pthread_mutex_t mutex;  // Mutex para sincronización
};

// Declaración de la variable global head_p
struct list_node_s* head_p = NULL;

int Delete(int value, struct list_node_s** head_p);
int Member(int value);
int Insert(int value);

// Función para eliminar un nodo
int Delete(int value, struct list_node_s** head_p) {
    struct list_node_s* curr_p = *head_p;
    struct list_node_s* pred_p = NULL;

    if (curr_p != NULL)
        pthread_mutex_lock(&(curr_p->mutex));

    while (curr_p != NULL && curr_p->data < value) {
        if (curr_p->next != NULL)
            pthread_mutex_lock(&(curr_p->next->mutex)); 

        if (pred_p != NULL)
            pthread_mutex_unlock(&(pred_p->mutex));

        pred_p = curr_p; 
        curr_p = curr_p->next; 
    }

    if (curr_p != NULL && curr_p->data == value) {
        if (pred_p == NULL) {
            *head_p = curr_p->next;
        } else {
            pred_p->next = curr_p->next;
        }
        pthread_mutex_unlock(&(curr_p->mutex));
        free(curr_p);
        return 1;
    }

    if (curr_p != NULL)
        pthread_mutex_unlock(&(curr_p->mutex));
    if (pred_p != NULL)
        pthread_mutex_unlock(&(pred_p->mutex));

    return 0;
}

// Función para verificar si un elemento es miembro de la lista
int Member(int value) {
    struct list_node_s* temp_p;

    if (head_p == NULL)
        return 0;
    
    pthread_mutex_lock(&(head_p->mutex));
    temp_p = head_p;

    while (temp_p != NULL && temp_p->data < value) {
        if (temp_p->next != NULL)
            pthread_mutex_lock(&(temp_p->next->mutex));

        pthread_mutex_unlock(&(temp_p->mutex));
        temp_p = temp_p->next;
    }

    if (temp_p == NULL || temp_p->data > value) {
        if (temp_p != NULL)
            pthread_mutex_unlock(&(temp_p->mutex));
        return 0;
    } else {
        pthread_mutex_unlock(&(temp_p->mutex));
        return 1;
    }
}

// Función para insertar un nodo
int Insert(int value) {
    struct list_node_s* curr_p = head_p;
    struct list_node_s* pred_p = NULL;
    struct list_node_s* temp_p;

    if (head_p != NULL)
        pthread_mutex_lock(&(head_p->mutex));

    while (curr_p != NULL && curr_p->data < value) {
        pred_p = curr_p;
        curr_p = curr_p->next;

        if (curr_p != NULL)
            pthread_mutex_lock(&(curr_p->mutex));
        
        pthread_mutex_unlock(&(pred_p->mutex));
    }

    temp_p = (struct list_node_s*)malloc(sizeof(struct list_node_s));
    if (temp_p == NULL) {
        fprintf(stderr, "Error de asignación de memoria\n");
        if (head_p != NULL)
            pthread_mutex_unlock(&(head_p->mutex));
        return -1;
    }
    temp_p->data = value;
    temp_p->next = curr_p;
    pthread_mutex_init(&(temp_p->mutex), NULL);

    if (pred_p == NULL) {
        head_p = temp_p;
    } else {
        pred_p->next = temp_p;
        pthread_mutex_unlock(&(pred_p->mutex));
    }

    if (curr_p != NULL)
        pthread_mutex_unlock(&(curr_p->mutex));

    return 1; 
}

// Estructura para los parámetros de los hilos
struct thread_data {
    int id;
    int num_insert_elements;
    int num_search_elements;
    int *elements;
    double insertion_time;
    double search_time;
    double total_time;
};

// Función que ejecuta cada hilo para insertar elementos
void* thread_insert(void* arg) {
    struct thread_data* data = (struct thread_data*)arg;
    int id = data->id;
    int num_elements = data->num_insert_elements;

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_time);

    for (int i = 0; i < num_elements; i++) {
        Insert(id * num_elements + i);
    }

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_time);

    data->insertion_time = (end_time.tv_sec - start_time.tv_sec) + 
                           (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    return NULL;
}

// Función que ejecuta cada hilo para buscar elementos
void* thread_search(void* arg) {
    struct thread_data* data = (struct thread_data*)arg;
    int id = data->id;
    int num_elements = data->num_search_elements;

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_time);

    for (int i = 0; i < num_elements; i++) {
        Member(data->elements[i]);
    }

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_time);

    data->search_time = (end_time.tv_sec - start_time.tv_sec) + 
                        (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    data->total_time = data->insertion_time + data->search_time;

    return NULL;
}

int main() {
    head_p = NULL;

    const int ths = 16;
    const int total_elements = 1000;
    const int elements_per_thread = total_elements / ths;

    pthread_t threads[ths];
    struct thread_data thread_args[ths];

    for (int i = 0; i < ths; i++) {
        thread_args[i].id = i;
        thread_args[i].num_insert_elements = elements_per_thread;
        pthread_create(&threads[i], NULL, thread_insert, (void*)&thread_args[i]);
    }

    for (int i = 0; i < ths; i++) {
        pthread_join(threads[i], NULL);
    }

    const int consulta = 100000;
    int elements_to_search[consulta];

    for (int i = 0; i < consulta; i++) {
        elements_to_search[i] = i * (total_elements / consulta);
    }

    for (int i = 0; i < ths; i++) {
        thread_args[i].id = i;
        thread_args[i].num_search_elements = consulta / ths;
        thread_args[i].elements = elements_to_search;
        pthread_create(&threads[i], NULL, thread_search, (void*)&thread_args[i]);
    }

    for (int i = 0; i < ths; i++) {
        pthread_join(threads[i], NULL);
    }

    double total_time_all_threads = 0.0;
    for (int i = 0; i < ths; i++) {
        total_time_all_threads += thread_args[i].total_time;
    }

    printf("Tiempo total de todos los hilos: %f segundos\n", total_time_all_threads);

    struct list_node_s* current = head_p;
    struct list_node_s* next;
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }

    return 0;
}
