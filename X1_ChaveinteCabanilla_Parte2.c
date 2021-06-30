#include<stdio.h>
#include<pthread.h>
#include <semaphore.h>
#include<stdlib.h>
#include<stdbool.h>

// MIGUEL CHAVEINTE GARCIA DNI:70840164A
// JHON STEEVEN CABANILLA ALVARADO DNI:71056366M

//Declaracion tipos de datos
struct datoBuffer
{
        char m_dec;
        char m_uni;
        char m_letras;
        bool m_bFin;
};

struct datoVector
{
        int m_procesados;
        int m_correctos;
		int m_indMax;
};

struct tNodo
{
	int m_pos;
	char m_char;
	struct tNodo *m_sig;
};

//Declaracion funciones
void inserta(int pos, char car);
void recorre(FILE* fichEscritura);

//Variables Globales
struct datoBuffer *buffer1;
struct tNodo* lista = NULL;
struct datoVector *vectorDato;

int tamBuffer;
int numConsumidores;
int indexC=0;
int hiloterminado;

//Semaforos
sem_t hay_dato;
sem_t hay_espacio;
sem_t mutexC;
sem_t hayDatoparaLector;
sem_t hayEspacioparaLector;
sem_t mutexLista;


//Hilo productor
void *productor(void *arg)
{	
        //Fichero abierto en el main
        FILE *fichero=(FILE *) arg;

        char cadena [7];
        struct datoBuffer dato;
        int index = 0; //sig posicion a llenar

        while(fgets(cadena, 100, fichero) != NULL)
        {
                int cuentaCaracteres=0;
                int i=0;
                while (cadena[i] != 13) {
                        cuentaCaracteres++;
                        i++;
                }
                if (cuentaCaracteres == 3)
                {
                        dato.m_dec =  cadena[0];
                        dato.m_uni = cadena[1];
						dato.m_letras = cadena[2];
                        dato.m_bFin = false;

						//Escribir en el Buffer circular
                        sem_wait(&hay_espacio);
                        buffer1[index] = dato;
                        index = (index + 1) % tamBuffer;
                        sem_post(&hay_dato);
                }
        }
		//Al ser el ultimo token llevar señal de fin
        sem_wait(&hay_espacio);
        dato.m_bFin = true;
        buffer1[index] = dato;
        sem_post(&hay_dato);
        pthread_exit(NULL);
}
//Hilo consumidor
void *consumidor(void *arg)
{
        int *id=(int *)arg;
		//Variables para escritura en fichero
		int numProcesados;
		int numCorrectos;
		int maximo;
		
		struct datoVector datoProcesado;
        struct datoBuffer dato;
        int posicion;
        while(true)
        {
				//Leer del Buffer circular
                sem_wait(&hay_dato);
                sem_wait(&mutexC);
                dato = buffer1[indexC];
				numProcesados ++;
				
                if(dato.m_bFin != true) {
                        indexC = (indexC + 1) % tamBuffer;
                }
                else{
                        sem_post(&mutexC);
                        sem_post(&hay_dato);
                        break;
                }
                sem_post(&mutexC);
                sem_post(&hay_espacio);

				//Validacion de token valido
                if(dato.m_dec >= 'd' && dato.m_dec <= 'm')
                {
                        posicion = (((int)dato.m_dec) - 100) *10;
                }
                if(dato.m_uni >= 'F' && dato.m_uni <= 'O')
                {
                        posicion=posicion+(((int)dato.m_uni) -70);
						numCorrectos ++;
						if (posicion > maximo)
							maximo = posicion;
						//Inserta nodo a la lista enlazada con semaforo para no ser interrumpido
						sem_wait(&mutexLista);
						inserta(posicion,dato.m_letras-1);
						sem_post(&mutexLista);
                }
           }
		   
		datoProcesado.m_procesados = numProcesados;
		datoProcesado.m_correctos = numCorrectos;
		datoProcesado.m_indMax = maximo;
		
		//Guardamos los datos del hilo en vector
		vectorDato[*id] = datoProcesado;
		sem_wait(&hayEspacioparaLector);
		hiloterminado = *id;
		sem_post(&hayDatoparaLector);
 }

//Hilo lector
void *lector(void *arg)
{
		FILE *fichEscritura=(FILE *)arg;
		struct datoVector datoSalida;
		int indexL;
		int procesadosTotales=0;
		int procesadosCorrectos=0;
		int indexSupremo=0;
		for (int i=0; i<numConsumidores; i++)	//Por cada consumidor
		{
			sem_wait(&hayDatoparaLector);
			indexL = hiloterminado;	
			sem_post(&hayEspacioparaLector);
			datoSalida = vectorDato[indexL];
			//Escritura en fichero por cada hilo y sus atributos.
			fprintf(fichEscritura,"%s%d\n","HILO ",indexL);
			fprintf(fichEscritura,"\t%s%d\n","Tokens procesados: ",datoSalida.m_procesados);
			fprintf(fichEscritura,"\t%s%d\n","Tokens correctos: ",datoSalida.m_correctos);
			fprintf(fichEscritura,"\t%s%d\n","Tokens incorrectos: ",datoSalida.m_procesados-datoSalida.m_correctos);
			fprintf(fichEscritura,"\t%s%d\n","Max index: ",datoSalida.m_indMax);
			procesadosTotales+=datoSalida.m_procesados;
			procesadosCorrectos+=datoSalida.m_correctos;
			if(datoSalida.m_indMax>indexSupremo)
			{
				indexSupremo=datoSalida.m_indMax;
			}
		}
		fprintf(fichEscritura,"%s\n","Resultado final (los que procesa el consumidor final)  ");
		fprintf(fichEscritura,"\t%s%d\n","Tokens procesados: ",procesadosTotales);
		fprintf(fichEscritura,"\t%s%d\n","Tokens correctos: ",procesadosCorrectos);
		fprintf(fichEscritura,"\t%s%d\n","Tokens incorrectos: ",procesadosTotales-procesadosCorrectos);
		fprintf(fichEscritura,"\t%s%d\n","Max index: ",indexSupremo);
		if(procesadosCorrectos-indexSupremo==1)
		{
			fprintf(fichEscritura,"\t%s\n","Mensaje: Correcto!");
			//Se escribe el mensaje descifrado.
			recorre(fichEscritura);
		}
		else
		{
			fprintf(fichEscritura,"\t%s\n","Mensaje: Incorrecto :(");
		}
}

//Funcion que recorre la lista enlazada y escribe en fichero el char correspondiente
void recorre(FILE* fichEscritura)
{
	struct tNodo* index = lista; //Indice auxiliar para recorrer la lista
	fprintf(fichEscritura,"%s","Mensaje traducido:");
	while(index != NULL)
	{
		fprintf(fichEscritura, "%c", index -> m_char);
		index = index -> m_sig; 
	}
	fprintf(fichEscritura,"\n");	
}

//Funcion que inserta un nuevo nodo a la lista enlazada en la posicion correspondiente
void inserta(int pos, char car)
{
	struct tNodo* index = lista;
	struct tNodo* anterior = NULL;
	
	while(index != NULL && pos > index->m_pos)
	{
		anterior = index;
		index = index -> m_sig; 
	}
	
	struct tNodo* nodo = (struct tNodo*)malloc(sizeof(struct tNodo)); //Nodo auxiliar
	if (nodo == NULL)
    {
        printf("ERROR:No se pudo reservar correctamente la memoria\n");
        exit(-1);
    }
	nodo -> m_pos = pos;
	nodo -> m_char = car;
	
	//Actualizar nodos
	if (anterior == NULL)	//Caso especial en caso de que el bucle no de ninguna vuelta
		lista = nodo;
	else
		anterior -> m_sig = nodo;
	nodo -> m_sig = index;		
}

//Main
int main(int argc, char* argv[])
{
        //Comprobacion de numero de parametros correcto
        if (argc != 5)
        {
                printf("ERROR:Numero de parametros invalido\n");
                return -1;
        }
        FILE* fichEntrada;
        fichEntrada = fopen(argv[1], "r");
        //Comprobacion de fichero vacio
        if(fichEntrada==(FILE *) NULL)
        {
                printf("ERROR:El fichero no pudo abrirse para lectura\n");
                return -1;
        }
        

		FILE* fichSalida;
        fichSalida = fopen(argv[2], "wt");
        //Comprobacion de fichero vacio
        if(fichSalida==(FILE *) NULL)
        {
                printf("ERROR:El fichero no pudo abrirse para escritura\n");
                return -1;
        }
        //Leer tamaño del Buffer y convertirlo en un entero
        if (sscanf(argv[3], "%d", &tamBuffer) != 1)
        {
             printf("ERROR:El tamaño  del buffer no es entero");
             return -1;
        }

        //validar que el tamBuffer es >0
        if (tamBuffer <= 0)
        {
                printf("ERROR:El tamaño del Buffer no puede ser negativo ni igual a 0\n");
                return -1;
        }

        if (sscanf(argv[4], "%d", &numConsumidores) != 1)
        {
             printf("ERROR:El numero de consumidores  no es entero");
             return -1;
        }

        if(numConsumidores<=0)
        {
                printf("ERROR:El numero de consumidores no puede ser negativo ni igual a 0\n");
                return -1;
        }

        //Reservar memoria de forma dinamica
        buffer1 = (struct datoBuffer *)malloc(tamBuffer*sizeof(struct datoBuffer));
        if (buffer1 == NULL)
        {
                printf("ERROR:No se pudo reservar correctamente la memoria\n");
                return -1;
        }

		vectorDato = (struct datoVector *)malloc(numConsumidores*sizeof(struct datoVector));
        if (vectorDato  == NULL)
        {
                printf("ERROR:No se pudo reservar correctamente la memoria\n");
                return -1;
        }

        //Inicializacion de semaforos
        sem_init (&hay_dato, 0, 0);
        sem_init (&hay_espacio, 0, tamBuffer);
        sem_init(&mutexC,0,1);
		sem_init (&hayDatoparaLector, 0, 0);
		sem_init (&hayEspacioparaLector, 0, 1);
		sem_init(&mutexLista,0,1);
		
        //Declaracion de hilos: productor y consumidor
        pthread_t tproductor;
        pthread_t tconsumidor[numConsumidores];
		pthread_t tlector;

        int id [numConsumidores];
		//Creacion de hilos
        pthread_create(&tproductor, NULL, productor,(void *)fichEntrada);
		pthread_create(&tlector, NULL, lector, (void *)fichSalida);
		
        for(int i =0;i<numConsumidores;i++){
                id[i]=i;
                pthread_create(&tconsumidor[i],NULL,*consumidor,(void *) &id[i]);
        }
        pthread_join(tproductor, NULL);
		
        for(int i =0;i<numConsumidores;i++){
                pthread_join(tconsumidor[i],NULL);
        }
		
		pthread_join(tlector, NULL);


        //Liberar memoria
        free(buffer1);
		free(vectorDato);

		fclose(fichEntrada);
		fclose(fichSalida);

        sem_destroy(&hay_dato);
        sem_destroy(&hay_espacio);
        sem_destroy(&mutexC);
		sem_destroy(&hayDatoparaLector);
		sem_destroy(&hayEspacioparaLector);
		sem_destroy(&mutexLista);
        return 0;
}


                                                                                                                                   