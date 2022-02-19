# BlockchainProject

Utilizzo: 
- `make -f Makefile`
- `./bin/Master settings.conf`

### Importante
In caso di crash/bug/chiusura forzata:
1. `gnome-system-monitor`: controllate i processi, cercate **Master** con la lente e nel caso chiudeteli tutti
2. `ipcs`: controllate gli oggetti IPC presenti
3. `ipcrm --all`: elimina tutti gli oggetti IPC (<ins>IMPORTANTE</ins>)
    - indicativamente non dovreste avere nessun Semaphore o Queue presente prima di runnare, mentre le Shared Memory potrebbero essere presenti:
        - se sono di sistema 
        - rimasugli di precedenti crash e il loro processo fosse ancora attivo (ricontrollate col passo 1)
