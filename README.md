┌─────────────────────────────────────────────────────────────────┐
│                     FUNÇÕES PRINCIPAIS                          │
└─────────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┴────────────────┐
              │                                │
┌─────────────▼──────────────┐   ┌────────────▼─────────────────┐
│          CLIENTE           │   │          SERVIDOR            │
│  socketsUtilsCliente.c     │   │   socketsUtilsServidor.c     │
└────────────────────────────┘   └──────────────────────────────┘
              │                                │
              │                                │
              │                  ┌─────────────┴─────────────┐
              │                  │                           │
              │            INICIALIZAÇÃO              SINCRONIZAÇÃO
              │                  │                           │
              │                  │                           │
              │         ┌────────▼─────────┐    ┌───────────▼──────────┐
              │         │ initSharedMemory()│    │   readerLock()       │
              │         │ [socketsUtils     │    │   readerUnlock()     │
              │         │  Servidor.c]      │    │   writerLock()       │
              │         └──────────────────┘    │   writerUnlock()     │
              │                                  │   [socketsUtils      │
              │                                  │    Servidor.c]       │
              │                                  └──────────────────────┘
              │
              │
    ┌─────────┴──────────┐              ┌────────────────────────────┐
    │                    │              │     GESTÃO DE CLIENTES     │
    │                    │              │                            │
┌───▼──────────────┐ ┌───▼──────────┐  │  ┌──────────────────────┐  │
│  fillSudoku()    │ │writeClient   │  │  │registerUniqueClient()│  │
│  [socketsUtils   │ │Stats()       │  │  │  ├─► writerLock()    │  │
│   Cliente.c]     │ │[socketsUtils │  │  │  └─► writerUnlock()  │  │
│                  │ │ Cliente.c]   │  │  │  [socketsUtils       │  │
│  ├─► Loop jogo   │ │              │  │  │   Servidor.c]        │  │
│  ├─► stats++     │ │├─►Calcula    │  │  └──────────────────────┘  │
│  │   local       │ ││  métricas   │  │                            │
│  │               │ │└─►fopen("w") │  │  ┌──────────────────────┐  │
│  └─►Chama verify │ │   fprintf()  │  │  │unregister            │  │
│     Answer       │ │   fclose()   │  │  │ClientOnline()        │  │
│     (servidor)   │ │              │  │  │  ├─► writerLock()    │  │
└──────────────────┘ └──────────────┘  │  │  └─► writerUnlock()  │  │
                                        │  │  [socketsUtils       │  │
                                        │  │   Servidor.c]        │  │
                     ┌──────────────────┼──┴──────────────────────┘  │
                     │                  │                            │
                     │   JOGO E STATS   │  ┌──────────────────────┐  │
                     │                  │  │increment             │  │
        ┌────────────▼────────────┐     │  │ActiveClients()       │  │
        │ sendGameToClient()      │     │  │  ├─► writerLock()    │  │
        │ [socketsUtilsServidor.c]│     │  │  └─► writerUnlock()  │  │
        │                         │     │  │  [socketsUtils       │  │
        │ ├─► Escolhe jogo        │     │  │   Servidor.c]        │  │
        │ ├─► Envia socket        │     │  └──────────────────────┘  │
        │ ├─► writerLock()        │     │                            │
        │ ├─► totalJogos++        │     │  ┌──────────────────────┐  │
        │ ├─► clienteJogos[i]++   │     │  │decrement             │  │
        │ ├─► writerUnlock()      │     │  │ActiveClients()       │  │
        │ └─► updateRealtimeStats()│    │  │  ├─► writerLock()    │  │
        └─────────┬───────────────┘     │  │  └─► writerUnlock()  │  │
                  │                     │  │  [socketsUtils       │  │
        ┌─────────▼────────────┐        │  │   Servidor.c]        │  │
        │ verifyClient         │        │  └──────────────────────┘  │
        │ SudokuAnswer()       │        │                            │
        │ [socketsUtils        │        │  ┌──────────────────────┐  │
        │  Servidor.c]         │        │  │getActiveClients      │  │
        │                      │        │  │Count()               │  │
        │ ├─► Valida resposta  │        │  │  ├─► readerLock()    │  │
        │ ├─► writerLock()     │        │  │  ├─► Lê contador     │  │
        │ ├─► totalAcertos++   │        │  │  └─► readerUnlock()  │  │
        │ │   ou totalErros++  │        │  │  [socketsUtils       │  │
        │ ├─► clienteAcertos[i]│        │  │   Servidor.c]        │  │
        │ │   ou Erros[i]++    │        │  └──────────────────────┘  │
        │ ├─► writerUnlock()   │        └────────────────────────────┘
        │ ├─► Envia "Correct"/ │
        │ │   "Wrong"           │
        │ └─► updateRealtimeStats()
        └──────────┬────────────┘
                   │
        ┌──────────▼────────────────────┐
        │ updateRealtimeStats()         │
        │ [socketsUtilsServidor.c]      │
        │                               │
        │ ├─► readerLock()              │
        │ ├─► Lê clientesAtivos         │
        │ ├─► Loop todos os clientes    │
        │ │   └─►if(clienteOnline[i]==1)│
        │ │      ├─►Soma acertos         │
        │ │      ├─►Soma erros           │
        │ │      ├─►Soma jogos           │
        │ │      └─►Calcula melhor       │
        │ ├─► readerUnlock()             │
        │ ├─► Calcula métricas           │
        │ │   (taxaAcerto, médias)       │
        │ └─► fopen("w")                 │
        │     fprintf() 8 métricas       │
        │     fclose()                   │
        │     [estatisticas_servidor.csv]│
        └────────────────────────────────┘


┌─────────────────────────────────────────────────────────────────┐
│                         LIMPEZA                                 │
└─────────────────────────────────────────────────────────────────┘
                              │
                ┌─────────────▼──────────────┐
                │ cleanupSharedMemory()      │
                │ [socketsUtilsServidor.c]   │
                │                            │
                │ ├─► munmap(sharedMem)      │
                │ └─► shm_unlink(SHM_NAME)   │
                └────────────────────────────
