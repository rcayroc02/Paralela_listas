#include <stdio.h>      // Para funciones de entrada/salida
#include <stdlib.h>     // Para funciones de manejo de memoria
#include <pthread.h>    // Para funciones de manejo de hilos, mutex y read-write locks
#include <time.h>       // Para medir el tiempo

// Definición de la estructura para los nodos de una lista enlazada
struct list_node_s {
    int data;
    struct list_node_s* next;
};

// Declaración de la variable global head_p y el read-write lock para la lista
struct list_node_s* head_p = NULL;
pthread_rwlock_t rwlock;  // Read-write lock para proteger toda la lista

int Delete(int value);
int Member(int value);
int Insert(int value);

// Función para eliminar un nodo (write lock)
int Delete(int value) {
    pthread_rwlock_wrlock(&rwlock); // Bloquear con write lock
    struct list_node_s* curr_p = head_p;
    struct list_node_s* pred_p = NULL;

    while (curr_p != NULL && curr_p->data < value) {
        pred_p = curr_p;
        curr_p = curr_p->next;
    }

    // Si se encontró el nodo a eliminar
    if (curr_p != NULL && curr_p->data == value) {
        if (pred_p == NULL) { // Deleting the first node
            head_p = curr_p->next; // Update head pointer
        } else {
            pred_p->next = curr_p->next; // Bypass the current node
        }
        free(curr_p); // Free the memory of the deleted node
        pthread_rwlock_unlock(&rwlock); // Desbloquear el write lock
        return 1; // Successful deletion
    }

    pthread_rwlock_unlock(&rwlock); // Desbloquear el write lock
    return 0; // Value not found in the list
}

// Función para verificar si un elemento es miembro de la lista (read lock)
int Member(int value) {
    pthread_rwlock_rdlock(&rwlock); // Bloquear con read lock
    struct list_node_s* temp_p = head_p;

    while (temp_p != NULL && temp_p->data < value) {
        temp_p = temp_p->next;
    }

    if (temp_p == NULL || temp_p->data > value) {
        pthread_rwlock_unlock(&rwlock); // Desbloquear el read lock
        return 0; // No encontrado
    } else {
        pthread_rwlock_unlock(&rwlock); // Desbloquear el read lock
        return 1; // Encontrado
    }
}

// Función para insertar un nodo (write lock)
int Insert(int value) {
    pthread_rwlock_wrlock(&rwlock); // Bloquear con write lock
    struct list_node_s* temp_p = (struct list_node_s*)malloc(sizeof(struct list_node_s));
    if (temp_p == NULL) {
        fprintf(stderr, "Error de asignación de memoria\n");
        pthread_rwlock_unlock(&rwlock); // Desbloquear el write lock
        return -1;
    }
    temp_p->data = value;
    temp_p->next = NULL;

    // Insertar en la lista ordenada
    if (head_p == NULL || head_p->data > value) {
        temp_p->next = head_p;
        head_p = temp_p;
    } else {
        struct list_node_s* curr_p = head_p;
        while (curr_p->next != NULL && curr_p->next->data < value) {
            curr_p = curr_p->next;
        }
        temp_p->next = curr_p->next;
        curr_p->next = temp_p;
    }

    pthread_rwlock_unlock(&rwlock); // Desbloquear el write lock
    return 1;
}

// Estructura para los parámetros de los hilos
struct thread_data {
    int id;
    int num_insert_elements; // Número de elementos a insertar
    int num_search_elements; // Número de elementos a buscar
    int *elements;           // Elementos a buscar
    double insertion_time;   // Tiempo tomado por la inserción
    double search_time;      // Tiempo tomado por la búsqueda
    double total_time;       // Tiempo total (inserción + búsqueda)
};

// Función que ejecuta cada hilo para insertar elementos
void* thread_insert(void* arg) {
    struct thread_data* data = (struct thread_data*)arg;
    int id = data->id;
    int num_elements = data->num_insert_elements;

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_time); // Inicio del tiempo

    // Cada hilo inserta `num_elements` valores
    for (int i = 0; i < num_elements; i++) {
        Insert(id * num_elements + i); // Insertar valores secuenciales
    }

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_time); // Fin del tiempo

    // Calcular el tiempo tomado en segundos
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
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_time); // Inicio del tiempo

    // Cada hilo busca `num_elements` valores
    for (int i = 0; i < num_elements; i++) {
        Member(data->elements[i]); // Buscar el elemento
    }

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_time); // Fin del tiempo

    // Calcular el tiempo tomado en segundos
    data->search_time = (end_time.tv_sec - start_time.tv_sec) + 
                        (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    // Calcular el tiempo total
    data->total_time = data->insertion_time + data->search_time;

    return NULL;
}

int main() {
    // Inicialización del nodo cabeza y del read-write lock
    head_p = NULL;
    pthread_rwlock_init(&rwlock, NULL); // Inicializar el read-write lock

    const int ths = 16;       // Número de hilos
    const int total_elements = 1000; // Total de elementos a insertar
    const int elements_per_thread = total_elements / ths; // Elementos por hilo

    // Insertar 1000 elementos en la lista enlazada
    pthread_t threads[ths];
    struct thread_data thread_args[ths];

    for (int i = 0; i < ths; i++) {
        thread_args[i].id = i;
        thread_args[i].num_insert_elements = elements_per_thread; // Cada hilo inserta 250 elementos
        pthread_create(&threads[i], NULL, thread_insert, (void*)&thread_args[i]);
    }

    // Esperar a que terminen los hilos de inserción
    for (int i = 0; i < ths; i++) {
        pthread_join(threads[i], NULL);
    }

    // Preparar los elementos para buscar
    const int consulta = 100000; // Número de elementos a buscar
    int elements_to_search[consulta];

    // Rellenar el array con elementos secuenciales para la búsqueda
    for (int i = 0; i < consulta; i++) {
        elements_to_search[i] = i * (total_elements / consulta); // Buscar cada n-ésimo elemento
    }

    // Hilos para búsqueda
    for (int i = 0; i < ths; i++) {
        thread_args[i].id = i;
        thread_args[i].num_search_elements = consulta / ths; // Cada hilo busca elementos equitativamente
        thread_args[i].elements = elements_to_search; // Pasar el array de búsqueda
        pthread_create(&threads[i], NULL, thread_search, (void*)&thread_args[i]);
    }

    // Esperar a que terminen los hilos de búsqueda
    for (int i = 0; i < ths; i++) {
        pthread_join(threads[i], NULL);
    }

    // Variable para almacenar el tiempo total de todos los hilos
    double total_time_all_threads = 0;

    // Imprimir los tiempos de inserción, búsqueda y total
    for (int i = 0; i < ths; i++) {
        //printf("Tiempo total del hilo %d: %f segundos\n", thread_args[i].id, thread_args[i].total_time);
        total_time_all_threads += thread_args[i].total_time;  // Sumar el tiempo total de cada hilo
    }

    // Imprimir el tiempo total sumado de todos los hilos
    printf("Tiempo total de todos los hilos: %f segundos\n", total_time_all_threads);

    // Limpiar la memoria de la lista enlazada antes de salir
    struct list_node_s* current = head_p;
    struct list_node_s* next;
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }

    pthread_rwlock_destroy(&rwlock); // Destruir el read-write lock
    return 0;
}
