# Tests Intelligent Ambience

Ces tests utilisent des entrées synthétiques, sans capture réelle ni commande
de périphérique. Python sert uniquement à lancer le compilateur et les tests ;
le moteur et l'effet de production sont natifs. Utiliser une invite développeur
**MSVC x64**, depuis la racine de ce dépôt.

## Vidéo CPU

```bat
python tests\intelligent-ambience\run_video.py
```

Le runner compile `VideoEngine.cpp` et `video_cpu_tests.cpp` en C++17
`/O2 /W4 /WX`, sans Qt. Le jeu CPU du prototype a été conservé intégralement,
avec seulement l'include adapté à la copie intégrée dans Effects.

Validation du 27 septembre 2026 : **462 assertions PASS**, aucun avertissement.
Couverture : lumière linéaire, quatre bords, trajectoires après sortie, TTL,
HUD fixe, flash/coupe, caméra, époques retardées, sauts de temps/résolution,
révision de layout, ownership des pixels et requêtes scalaires/batch.
Les assertions d'acceptation répétées des trames ne sont pas autant de
scénarios indépendants. Le programme imprime aussi des mesures CPU locales ;
elles ne mesurent pas la capture ni la latence jusqu'aux LED.

## Vidéo GPU

```bat
python tests\intelligent-ambience\run_video_gpu.py --qt C:\Qt\6.8.3\msvc2022_64 --core ..\OpenRGB-Room
```

Adapter `--qt` à une installation Qt6 MSVC x64 et `--core` au fork OpenRGB.
Le runner compile les vrais `ShaderProgram.cpp`, `ShaderPass.cpp` et
`VideoEngine.cpp`, puis charge `shaders/IntelligentAmbience/video.fs` dans
un contexte OpenGL hors écran. Aucun écran de l'utilisateur n'est lu.

Validation du 27 septembre 2026 : **60 checks PASS**, dont **27 comparaisons
d'images complètes CPU/GPU**, écart maximal **1/255 sRGB** (seuil du test : 2).
Les comparaisons couvrent orientation, bilinéaire, rectangle déplacé, aspect,
force/persistance, trajectoire, TTL, reset, coupe et image périmée. Les tests
supplémentaires vérifient les snapshots immuables, le heartbeat statique sans
upload ni prolongation des événements, ainsi que l'expiration réelle d'un ou
des deux plans GPU alors que les uniforms restent figés.

Les commandes des runners reprennent celles utilisées pour ces validations.
Lors de leur ajout, leur syntaxe et leurs arguments ont été vérifiés sans
relancer les tests GPU déjà passés. Les exécutables et objets sont isolés sous
`build/intelligent-ambience-video*`.

## Musique et interface

Voir [MUSIC.md](MUSIC.md), `run_music.py`, `run_music_gpu.py` et `run_ui.py`.
Leurs validations sont distinctes de la comparaison vidéo ci-dessus.
