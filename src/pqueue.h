
#include <pthread.h>

struct pqueue_elem {
	void * data;
	struct pqueue_elem * next;
} pqueue_elem;

struct pqueue {
	pthread_mutex_t queue_lock;
	pthread_cond_t  not_empty;
	int queue_end;
	int queue_size;
	struct pqueue_elem * qhead;
	struct pqueue_elem * qtail;
};

static void pqueue_init(struct pqueue * queue) {
	queue->queue_end = 0;
	queue->qhead = NULL;
	queue->qtail = NULL;
	queue->queue_size = 0;
	pthread_mutex_init(&queue->queue_lock, NULL);
	pthread_cond_init(&queue->not_empty, NULL);
}

static void pqueue_release(struct pqueue * queue) {
	pthread_mutex_lock(&queue->queue_lock);
	
	// Mark the queue as ended
	queue->queue_end = 1;
	// Unblock all threads
	pthread_cond_broadcast(&queue->not_empty);
	
	pthread_mutex_unlock(&queue->queue_lock);
}

static void pqueue_push(struct pqueue * queue, void * usr_data) {
	pthread_mutex_lock(&queue->queue_lock);

	// Create list node and fill it in
	struct pqueue_elem * ne = (struct pqueue_elem *)malloc(sizeof(pqueue_elem));
	ne->data = usr_data;
	ne->next = 0;

	// Non Null qtail pointer means we already have a ptr to the last elem
	// Null ptr means list is empty
	if (queue->qtail)
		queue->qtail->next = ne;
	else
		queue->qhead = ne;

	// qtail points to the newly created element
	queue->qtail = ne;

	queue->queue_size++;
	
	// Signal some other thread waiting in the queue
	pthread_cond_signal(&queue->not_empty);
	
	pthread_mutex_unlock(&queue->queue_lock);
}

// Push element in the front (typically used to give hi prio)
static void pqueue_push_front(struct pqueue * queue, void * usr_data) {
	pthread_mutex_lock(&queue->queue_lock);
		
	struct pqueue_elem * ne = (struct pqueue_elem *)malloc(sizeof(pqueue_elem));
	ne->data = usr_data;
	ne->next = queue->qhead;

	queue->qhead = ne;
	queue->queue_size++;

	// If list was empty now we have a last element
	if (!queue->qtail)
		queue->qtail = ne;
	
	// Signal some other thread waiting in the queue
	pthread_cond_signal(&queue->not_empty);
	
	pthread_mutex_unlock(&queue->queue_lock);
}

// Returns the fron element. If there is no element it blocks until there is any.
// If the writing end of the queue is closed it returns NULL
static void * pqueue_pop(struct pqueue * queue) {
	pthread_mutex_lock(&queue->queue_lock);
	
	void * udata = 0;
	while (1) {
		if (queue->qhead != 0) {
			struct pqueue_elem * h = queue->qhead;
			udata = queue->qhead->data;
		
			queue->qhead = queue->qhead->next;
			queue->queue_size--;
			if (!queue->qhead)
				queue->qtail = NULL;

			free(h);
		}
		
		if (udata == 0) { 
			if (queue->queue_end == 0) // No work in the queue, wait here!
				pthread_cond_wait(&queue->not_empty,&queue->queue_lock);
			else
				break;  // No work and we are told to finish working
		}
		else
			break;
	}
	
	pthread_mutex_unlock(&queue->queue_lock);
	
	return udata;
}

// Returns the element in the front or NULL if no element is present
// It never blocks!
static void * pqueue_pop_nonb(struct pqueue * queue) {
	pthread_mutex_lock(&queue->queue_lock);
	
	void * udata = 0;
	if (queue->qhead != 0) {
		struct pqueue_elem * h = queue->qhead;
		udata = queue->qhead->data;
	
		queue->qhead = queue->qhead->next;
		queue->queue_size--;
		if (!queue->qhead)
			queue->qtail = NULL;

		free(h);
	}
		
	pthread_mutex_unlock(&queue->queue_lock);
	
	return udata;
}

// Returns queue size
static int pqueue_size(struct pqueue * queue) {
	pthread_mutex_lock(&queue->queue_lock);
	
	int size = queue->queue_size;;
	
	pthread_mutex_unlock(&queue->queue_lock);
	
	return size;
}

// Returns whether the queue has been marked as released (writer finished)
static int pqueue_released(struct pqueue * queue) {
	pthread_mutex_lock(&queue->queue_lock);
	
	int res = queue->queue_end;
	
	pthread_mutex_unlock(&queue->queue_lock);
	
	return res;
}

// Sleeps until the queue has at least one element in it or the queue is released
static void pqueue_wait(struct pqueue * queue) {
	pthread_mutex_lock(&queue->queue_lock);
	
	while (queue->qhead == 0 && !queue->queue_end) {
		pthread_cond_wait(&queue->not_empty,&queue->queue_lock);
	}
	
	pthread_mutex_unlock(&queue->queue_lock);
}

