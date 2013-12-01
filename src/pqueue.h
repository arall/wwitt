
#include <pthread.h>

struct pqueue_elem {
	void * data;
	struct pqueue_elem * next;
} pqueue_elem;

struct pqueue {
	pthread_mutex_t queue_lock;
	pthread_cond_t  not_empty;
	int queue_end;
	struct pqueue_elem * qhead;
};

static void pqueue_init(struct pqueue * queue) {
	queue->queue_end = 0;
	queue->qhead = 0;
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
	struct pqueue_elem * h = queue->qhead;
	while (h && h->next != 0)
		h = h->next;
	
	struct pqueue_elem * ne = malloc(sizeof(pqueue_elem));
	ne->data = usr_data;
	ne->next = 0;

	if (h) h->next = ne;
	else   queue->qhead = ne;
	
	// Signal some other thread waiting in the queue
	pthread_cond_signal(&queue->not_empty);
	
	pthread_mutex_unlock(&queue->queue_lock);
}

static void * pqueue_pop(struct pqueue * queue) {
	pthread_mutex_lock(&queue->queue_lock);
	
	void * udata = 0;
	while (queue->queue_end == 0) {
		if (queue->qhead != 0) {
			struct pqueue_elem * h = queue->qhead;
			udata = queue->qhead->data;
		
			queue->qhead = queue->qhead->next;
			free(h);
		}
		
		if (udata == 0) // No work in the queue, wait here!
			pthread_cond_wait(&queue->not_empty,&queue->queue_lock);
		else
			break;
	}
	
	pthread_mutex_unlock(&queue->queue_lock);
	
	return udata;
}


static int pqueue_size(struct pqueue * queue) {
	pthread_mutex_lock(&queue->queue_lock);
	struct pqueue_elem * h = queue->qhead;
	
	int size = 0;
	while (h != 0) {
		h = h->next;
		size++;
	}
	
	pthread_mutex_unlock(&queue->queue_lock);
	
	return size;
}


