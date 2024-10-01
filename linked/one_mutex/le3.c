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

    // Lock the mutex of the head node if the list is not empty
    if (curr_p != NULL)
        pthread_mutex_lock(&(curr_p->mutex));

    while (curr_p != NULL && curr_p->data < value) {
        // Check if the next node exists and lock its mutex
        if (curr_p->next != NULL)
            pthread_mutex_lock(&(curr_p->next->mutex)); 

        // Unlock the previous node's mutex if it exists
        if (pred_p != NULL)
            pthread_mutex_unlock(&(pred_p->mutex));

        // Move to the next node
        pred_p = curr_p; 
        curr_p = curr_p->next; 
    }

    // If the current node's data matches the value to delete
    if (curr_p != NULL && curr_p->data == value) {
        if (pred_p == NULL) { // Deleting the first node
            *head_p = curr_p->next; // Update head pointer
        } else {
            pred_p->next = curr_p->next; // Bypass the current node
        }
        pthread_mutex_unlock(&(curr_p->mutex)); // Unlock the mutex of the current node
        free(curr_p); // Free the memory of the deleted node
        return 1; // Successful deletion
    }

    // Unlock remaining mutexes
    if (curr_p != NULL)
        pthread_mutex_unlock(&(curr_p->mutex)); // Unlock the current node if it's not NULL
    if (pred_p != NULL)
        pthread_mutex_unlock(&(pred_p->mutex)); // Unlock the previous node if it exists

    return 0; // Value not found in the list
}

// Función para verificar si un elemento es miembro de la lista
int Member(int value) {
    struct list_node_s* temp_p;

    // Verificar si head_p no es NULL antes de bloquear
    if (head_p == NULL)
        return 0;
    
    pthread_mutex_lock(&(head_p->mutex)); // Bloquear el mutex del nodo cabeza
    temp_p = head_p;

    while (temp_p != NULL && temp_p->data < value) {
        if (temp_p->next != NULL)
            pthread_mutex_lock(&(temp_p->next->mutex)); // Bloquear el mutex del siguiente nodo

        pthread_mutex_unlock(&(temp_p->mutex)); // Desbloquear el mutex del nodo actual
        temp_p = temp_p->next;
    }

    if (temp_p == NULL || temp_p->data > value) {
        if (temp_p != NULL)
            pthread_mutex_unlock(&(temp_p->mutex)); // Desbloquear el mutex si no es NULL
        return 0; // No encontrado
    } else {
        pthread_mutex_unlock(&(temp_p->mutex)); // Desbloquear el mutex si se encontró
        return 1; // Encontrado
    }
}

// Función para insertar un nodo
int Insert(int value) {
    struct list_node_s* curr_p = head_p;
    struct list_node_s* pred_p = NULL;
    struct list_node_s* temp_p;

    if (head_p != NULL)
        pthread_mutex_lock(&(head_p->mutex)); // Bloquear el mutex de la cabeza

    while (curr_p != NULL && curr_p->data < value) {
        pred_p = curr_p;
        curr_p = curr_p->next;

        if (curr_p != NULL)
            pthread_mutex_lock(&(curr_p->mutex)); // Bloquear el siguiente nodo
        
        pthread_mutex_unlock(&(pred_p->mutex)); // Desbloquear el nodo previo
    }

    // Crear un nuevo nodo
    temp_p = (struct list_node_s*)malloc(sizeof(struct list_node_s));
    if (temp_p == NULL) {
        fprintf(stderr, "Error de asignación de memoria\n");
        if (head_p != NULL)
            pthread_mutex_unlock(&(head_p->mutex));
        return -1;
    }
    temp_p->data = value;
    temp_p->next = curr_p;
    pthread_mutex_init(&(temp_p->mutex), NULL); // Inicializar el mutex correctamente

    // Ajustar los enlaces
    if (pred_p == NULL) { // Insertar al inicio de la lista
        head_p = temp_p;
    } else {
        pred_p->next = temp_p;
        pthread_mutex_unlock(&(pred_p->mutex)); // Desbloquear pred_p después de insertar
    }

    // Desbloquear el nodo actual si no es NULL
    if (curr_p != NULL)
        pthread_mutex_unlock(&(curr_p->mutex));

    return 1; 
}

void PrintList(struct list_node_s* head_p) {
    struct list_node_s* temp_p = head_p;

    // Traverse the list
    while (temp_p != NULL) {
        printf("%d -> ", temp_p->data);  // Print the data of the current node
        temp_p = temp_p->next;           // Move to the next node
    }
    printf("NULL\n");  // Indicate the end of the list
}

// Estructura para los parámetros de los hilos
struct thread_data {
    int id;
    int num_elements; // Número de elementos a insertar o buscar
    int *elements;    // Elementos a buscar
    double time_taken; // Tiempo tomado por el hilo
};

// Función que ejecuta cada hilo para insertar elementos
void* thread_insert(void* arg) {
    struct thread_data* data = (struct thread_data*)arg;
    int id = data->id;
    int num_elements = data->num_elements;

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_time); // Inicio del tiempo

    // Cada hilo inserta `num_elements` valores
    for (int i = 0; i < num_elements; i++) {
        Insert(id * num_elements + i); // Insertar valores secuenciales
    }

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_time); // Fin del tiempo

    // Calcular el tiempo tomado en segundos
    data->time_taken = (end_time.tv_sec - start_time.tv_sec) + 
                       (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    return NULL;
}

// Función que ejecuta cada hilo para buscar elementos
void* thread_search(void* arg) {
    struct thread_data* data = (struct thread_data*)arg;
    int id = data->id;
    int num_elements = data->num_elements;

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_time); // Inicio del tiempo

    // Cada hilo busca `num_elements` valores
    for (int i = 0; i < num_elements; i++) {
        Member(data->elements[i]); // Buscar el elemento
    }

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_time); // Fin del tiempo

    // Calcular el tiempo tomado en segundos
    data->time_taken = (end_time.tv_sec - start_time.tv_sec) + 
                       (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    return NULL;
}

int main() {
    // Inicialización del nodo cabeza
    head_p = NULL;

    const int ths = 4;       // Número de hilos
    const int total_elements = 1000; // Total de elementos a insertar
    const int elements_per_thread = total_elements / ths; // Elementos por hilo

    // Insertar 1000 elementos en la lista enlazada
    pthread_t threads[ths];
    struct thread_data thread_args[ths];

    for (int i = 0; i < ths; i++) {
        thread_args[i].id = i;
        thread_args[i].num_elements = elements_per_thread; // Cada hilo inserta 250 elementos
        pthread_create(&threads[i], NULL, thread_insert, (void*)&thread_args[i]);
    }

    // Esperar a que terminen los hilos de inserción
    for (int i = 0; i < ths; i++) {
        pthread_join(threads[i], NULL);
    }

    // Imprimir los tiempos de inserción
    for (int i = 0; i < ths; i++) {
        printf("Tiempo de inserción del hilo %d: %f segundos\n", thread_args[i].id, thread_args[i].time_taken);
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
        thread_args[i].num_elements = consulta / ths; // Cada hilo busca elementos equitativamente
        thread_args[i].elements = elements_to_search; // Pasar el array de búsqueda
        pthread_create(&threads[i], NULL, thread_search, (void*)&thread_args[i]);
    }

    // Esperar a que terminen los hilos de búsqueda
    for (int i = 0; i < ths; i++) {
        pthread_join(threads[i], NULL);
    }

    // Imprimir los tiempos de búsqueda
    for (int i = 0; i < ths; i++) {
        printf("Tiempo de búsqueda del hilo %d: %f segundos\n", thread_args[i].id, thread_args[i].time_taken);
    }

    // Imprimir la lista
    PrintList(head_p);

    // Limpiar la memoria de la lista enlazada antes de salir
    struct list_node_s* current = head_p;
    struct list_node_s* next;
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }

    return 0;
}
