# Reco-projekt-sysopy

Realizacja projektu nr 12 z przedmiotu Systemy Operacyjne semestr 4 rok 2 Informatyka WIET

Opis:
Celem projektu była implementacja własnego repozytorium kodu.
Program udostępnia następujące opcje:
-init
-pull
-status
-add
-push

Zalecane uruchomienie programu serwera w osobnym katalogu, np "repository".

Kompilacja i użycie:

-kompilacja:
  make
  
-użycie: server
  ./server 127.0.0.1
  
-użycie klient
  ./reco init folder NAZWA_CLI 127.0.0.1
  
  ./reco pull folder NAZWA_CLI 127.0.0.1
  
  ./reco push folder NAZWA_CLI 127.0.0.1
  
  ./reco status folder 
  
  ./reco add folder/plik.txt


Użyte mechanizmy:
  -sockety werjsa internetowa
  -rekurencyjne przeglądanie struktury katalogów (nftw)
  -wątki
