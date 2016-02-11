#include <unistd.h>
#include <stdio.h>

/* Taille d'un bloc lors d'une première initialisation */
#define DEFAULT_SIZE 4096

/* Taille que peut avoir au minimum un bloc */
#define MINIMUM_SIZE 2

/* Representation de l'entete d'un bloc memoire */
typedef struct bloc_s bloc_t;
struct bloc_s {
	
	/* Taille du bloc */
	size_t size;
	
	/* 1 = bloc libre et 0 si occupe */
	int free;
	
	/* Adresse du prochain bloc */
	bloc_t *next;
};

/* Debut de la zone memoire utilisable */
static void *mem_start 	= NULL;

/* Fin de la zone memorie utilisable */
static void *mem_end 	= NULL;

/**
 * Retourne le dernier bloc contenant tout l'espace libre 
 * @return Adresse du dernier bloc contenant tout l'espace libre
 */
bloc_t *find_last(){

	bloc_t *bloc;
	
	/* Petit controle pour savoir si l'initialisation est faite */
	if(mem_start == NULL){
		return NULL;
	}
	
	/* On parcourt les blocs a la chaine jusqu'a trouver le dernier */
	bloc = (bloc_t*) mem_start;
	while(bloc->next != NULL){
		bloc = bloc->next;
	}
	return bloc;

}

/**
 * Trouve un bloc libre avec une taille size au minimum
 * @param size Taille minimum du bloc recherche
 * @return Bloc adequat
 */
bloc_t *find_block(size_t size){
	
	bloc_t *bloc;
	
	/* Petit controle pour savoir si l'initialisation est faite */
	if(mem_start == NULL){
		return NULL;
	}
	
	/* On parcourt les blocs a la chaine jusqu'a trouver le bloc adequat */
	bloc = (bloc_t*) mem_start;
	while(bloc != NULL && (bloc->size < size || !bloc->free )){
		bloc = bloc->next;
	}
	return bloc;
	
}

/**
 * Deplace le break d'une certaine valeur donnee en parametre
 * et cree un nouveau bloc contenant toute la memoire utilisee
 * @param size Taille demandee pour le bloc
 * @return Le dernier bloc cree
 */	
bloc_t *more_mem(size_t size){
	
	/* Cas du premier bloc a creer */
	if(mem_start == mem_end){
		
		/* Debut de la memoire de travail */
		mem_start = sbrk(0);
		
		/* Allocation de la taille demandee */
		sbrk(size);
		
		/* Fin du bloc */
		mem_end = sbrk(0);
		
		/* Taille effective = taille allouee - zone de description */
		((bloc_t*)mem_start)->size = size - sizeof(bloc_t);
		
		/* Bloc libre au depart */
		((bloc_t*)mem_start)->free = 1;
		
		/* La premiere fois, il n'y a pas de suivant */
		((bloc_t*)mem_start)->next = NULL;
		
		return (bloc_t*) mem_start;
	
	} else { /* Cas ou il y a des blocs qui existent */

		/* On cherche l'adresse du dernier bloc */
		bloc_t * dernier = find_last();
		
		/* Attribution de la taille demandee */
		sbrk(size);
		
		/* Taille effective = taille demandee - taille de la zone de description */
		((bloc_t*)mem_end)->size = size - sizeof(bloc_t);
		
		/* Bloc libre au depart */
		((bloc_t*)mem_end)->free = 1;
		
		/* Pas de suivant */
		((bloc_t*)mem_end)->next = NULL;
		
		/* On attribue le bloc courant comme bloc suivant de celui existant */
		dernier->next = mem_end;
		
		/* Fin de la zone memoire */
		mem_end = sbrk(0);
		
		return dernier->next;
	}
}

/**
 * Decoupage d'un bloc passe en parametres en 2 blocs avec le premier de taille size
 * et le second avec l'espace restant
 * @return adresse du deuxieme bloc
 */
bloc_t *bloc_split(bloc_t *bloc, size_t size){
	
	/* Pour ne pas perdre de donnees, on stocke l'ancien next du bloc donne */
	bloc_t * ancien_next = bloc->next;
	
	/* On stocke l'ancienne taille du bloc pour ne pas la perdre */
	int ancienne_taille = bloc->size;
	
	/* On change la taille du bloc en celle passee en parametre */
	bloc->size = size - sizeof(bloc_t);
	
	/* Le nouveau bloc suivant se situe apres la taille demandee */
	bloc->next = (void *)bloc + size;
	
	/* La taille du deuxieme bloc = l'ancienne taille du bloc passe en param - la taille du premier bloc */
	bloc->next->size = ancienne_taille - size;
	
	/* Le bloc suivant du deuxieme bloc est le suivant du bloc en parametre */
	bloc->next->next = ancien_next;
	
	/* Deuxieme bloc libre */
	bloc->next->free = 1;
	
	/* Retourne l'adresse du deuxieme bloc */
	return bloc->next;
	
}

/**
 * Fonction d'allocation dynamique
 * @param size Taille a allouer
 * @return Adresse de la zone ou peut ecrire l'utilisateur
 */
void *malloc(size_t size){
	
	/* Dernier bloc attribue pour la demande de l'utilisateur */
	bloc_t * free_block;
	
	/* On verifie si l'allocateur a ete initialise */
	if(mem_start == NULL){
		
		/* On initialise les variables */
		mem_start = sbrk(0);
		mem_end = sbrk(0);
		
		/* On alloue un premier bloc */
		more_mem(DEFAULT_SIZE);
	}
	
	/* Recherche d'un bloc libre assez grand */
	free_block = find_block(size);
	
	/* Si aucun n'est trouve, on en cree un nouveau */
	if(free_block == NULL){
		
		/* Si la taille demandee est superieure a notre taille par defaut */
		if(size > DEFAULT_SIZE - sizeof(bloc_t)){
			
			/* Bloc de taille personnalisee */
			free_block = more_mem(size + sizeof(bloc_t));
			free_block->free = 0;
			
			/* On retourne directement le bloc */
			return (void*)free_block + sizeof(bloc_t);
		} else {
			
			/* Sinon, on cree un bloc de la taille par defaut */
			free_block = more_mem(DEFAULT_SIZE);
		}
	}
	
	/* On decoupe si c'est possible et utile puis on renvoie */
	if(free_block->size > (size + 2 * sizeof(bloc_t) + MINIMUM_SIZE)){
		bloc_split(free_block, size + sizeof(bloc_t));
	}
	
	free_block->free = 0;
	return (void*)free_block + sizeof(bloc_t);
}

/**
 * Liberation de la memoire
 * @param ptr Pointeur vers la zone memoire a liberer
 */
void free(void *ptr){
	
	/* Pointeur vers le bloc avant le bloc a liberer */
	bloc_t* bloc_av;
	
	/* Liberation de la memoire donnee */
	bloc_t* bloc = (void *)ptr - sizeof(bloc_t);
	bloc->free = 1;
	
	bloc_av = (bloc_t*)mem_start;
	
	/* On verifie si ce n'est pas le premier bloc qu'il est demande de liberer */
	if(bloc != bloc_av){
		
		/* Si ce n'est pas le premier bloc, on recherche son bloc precedent */
		while(bloc_av->next != NULL && bloc_av->next != bloc){
			bloc_av = bloc_av->next;
		}
		
		/* Si le bloc d'avant est libre, on fusionne */
		if(bloc_av->next != NULL && bloc_av->free){
			
			/* Nouvelle taille = ancienne taille + taille du bloc d'apres + en-tete libere du bloc d'apres */
			bloc_av->size += bloc->size + sizeof(bloc_t);
			
			/* Changement de suivant */
			bloc_av->next = bloc->next;
			bloc = bloc_av;
		}
	}
	
	/* On verifie si le bloc d'apres est libre pour fusionner */
	if(bloc->next != NULL && bloc->next->free){
		
		/* Calcul de la nouvelle taille = taille bloc1 + taille bloc2 + en-tete bloc2 */
		bloc->size += bloc->next->size + sizeof(bloc_t);
		
		/* Changement de suivant */
		bloc->next = bloc->next->next;
	}
	
}

/**
 * Reallocation d'une zone memoire pour une taille differente
 * @param ptr Zone memoire a etendre
 * @param new_size Nouvelle taille demandee
 * @return Pointeur vers la nouvelle zone memoire
 */
void *realloc(void *ptr, size_t new_size){
	
	bloc_t *bloc = (void *)ptr - sizeof(bloc_t);
	
	/* On ne fait les changements que si une nouvelle taille plus grande est demandee */
	if(bloc->size < new_size){
		
		/* Nouvelle zone memoire */
		void *nouveau = malloc(new_size);
		
		/*  Octet pointant vers une case de l'ancienne zone */
		char *oct_az = (char*) ptr;
		
		/* Octet pointant vers une case de la nouvelle zone */
		char *oct_nz = (char*) nouveau;
		
		/* Indice permettant le parcours */
		size_t i = bloc->size;
		
		/* Copie */
		while(i-- > 0){
			*oct_nz++ = *oct_az++;
		}
		
		/* Liberation de l'ancien bloc */
		free(ptr);
		
		return nouveau;
	}
	/* Dans tous les autres cas, on retourne le bloc donne */
	return ptr;
}

/**
 * Programme de test
 */
int main(){
	
	int *tab, *tab1, *tab2, i;
	
	printf("====================== Programme de test ======================\n");
	printf("\n\tTest classique, tableau de 10 entiers...\n\tRemplissage : ");
	
	tab = (int*)malloc(10 * sizeof(int));
	for(i = 0; i < 10; i++){
		tab[i] = 2*i;
		printf("#");
	}
	
	/* Affichage du tableau */
	printf("\n\ttab[] = ");
	for(i = 0; i < 10; i++){
		printf("%d | ", tab[i]);
	}
	printf("\n");
	
	/*
	 * On teste le cas ou il y a 3 zones memoires successives,
	 * Liberation de la zone 1 et 3 puis 2
	 * Pour regarder si la fusion se fait correctement, on realloue un bloc 
	 * de la taille des 3 et on regarde si le mem_end ne bouge pas
	 */
	printf("\n\tTest de la fusion, 3 blocs, liberation blocs 1, 3 et 2 dans cet ordre \n");
	tab1 = (int*)malloc(10 * sizeof(int));
	tab2 = (int*)malloc(10 * sizeof(int));
	
	printf("\tIci, 3 blocs existent, adresse fin de zone memoire : %p \n", mem_end);
	free(tab);
	free(tab2);
	free(tab1);
	printf("\tLiberation faite, adresse fin de zone memoire : %p \n", mem_end);
	tab = (int*)malloc(30 * sizeof(int));
	printf("\tReallocation, adresse fin de zone memoire : %p \n", mem_end);
	free(tab);
	
	/*
	 * Pour tester les fuites memoire, on alloue et desalloue une grande quantite de fois
	 * de la memoire. On va prendre une grande plage de memoire pour tester plus qu'une plage de 4096oct
	 */
	printf("\n\tTest des fuites de memoire \n");
	tab = (int*)malloc(5000);
	tab1 = (int*)malloc(5000);
	tab2 = (int*)malloc(5000);
	printf("\tPremiere allocation, adresse fin de zone memoire : %p \n", mem_end);
	for(i = 0; i < 60000; i++){
		free(tab);
		free(tab1);
		free(tab2);
		tab = (int*)malloc(5000);
		tab1 = (int*)malloc(5000);
		tab2 = (int*)malloc(5000);
	}
	printf("\tFin de la serie d'alloc/desalloc, adresse fin de zone memoire : %p \n", mem_end);
	free(tab);
	free(tab1);
	free(tab2);
		
	/*
	 * Test sur les contenus apres d'autres allocations affectations
	 * Pour verifier si les plages memoires donnees ne sont pas erronnees
	 */
	printf("\n\tTest des contenus des variables \n");
	tab = (int*)malloc(2*sizeof(int));
	tab[0] = 21311;
	tab[1] = 12345;
	printf("\tAvant 2eme allocation : tab[] = %d | %d \n", tab[0], tab[1]);
	tab1 = (int*)malloc(2*sizeof(int));
	tab1[0] = 15;
	tab1[1] = 18;
	printf("\t2eme allocation : tab1[] = %d | %d \n", tab1[0], tab1[1]);
	printf("\tApres 2eme allocation : tab[] = %d | %d \n", tab[0], tab[1]);
	free(tab);
	printf("\tApres liberation de tab : tab1[] = %d | %d \n", tab1[0], tab1[1]);
	free(tab1);
	
	printf("\n===============================================================\n");
	
	return 0;
}

