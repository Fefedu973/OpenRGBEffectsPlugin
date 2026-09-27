# Validation synthétique de l'apparence Better

Depuis une invite développeur MSVC, avec les chemins locaux adaptés :

```powershell
python tests/room-better-appearance/run.py --qt C:/Qt/6.8.3/msvc2022_64 --core ../OpenRGB-Room --better ../BetterSignalRGBScreenCapture
```

Le test compile les vrais `Appearance.cpp`, `ShaderRenderGraph.cpp` et `ShaderPass.cpp`.
Il utilise un contexte OpenGL hors écran ; aucun flux écran réel, périphérique RGB
ou service Better en cours d'exécution n'est ouvert.

Une comparaison supplémentaire peut utiliser le backend WebGL de la vraie page
Better, dans Chromium headless, avec des JPEG synthétiques uniquement :

```powershell
node tests/room-better-appearance/reference_webgl.cjs ../BetterSignalRGBScreenCapture build/room-better-appearance
```

Ce script nécessite Playwright déjà disponible. Il injecte son point d'observation
uniquement dans une copie en mémoire de la page ; le checkout Better reste intact.
Exécuter ensuite `run.py` pour comparer ces références supplémentaires.

Le dossier ignoré `build/room-better-appearance` contient les images et
`appearance-comparison.json` : erreurs moyennes/maximales, p99, proportion des canaux
avec erreur >8, divergence des masques, identité du renderer GL et benchmarks.

Les gates optiques sont explicites, en niveaux 0…255 :

- image/filtres sans halo : MAE <0,3 et maximum ≤8 ;
- Classic/Soft : MAE <1, p99 ≤4 et maximum ≤16 ;
- Contours : MAE <1,25, p99 ≤20 et moins de 3,5 % des canaux au-dessus de 8.

Contours conserve des erreurs ponctuelles importantes aux changements de frontière
antialiasée ; ces gates ne signifient pas pixel-perfect. Les chiffres détaillés
restent visibles dans le rapport. Le test couvre aussi la vraie topologie des trous
et recouvrements, les sources noires, les matrices non finies, les bornes mémoire,
l'absence de source et l'exécution GPU aux minima/maxima des trois styles.

Le benchmark utilise 800 × 600 : 6 préparations, puis 3 frames de chauffe et
24 frames avec nouveaux numéros de séquence. Les temps comprennent l'upload raw et
coverage, toutes les passes et le readback final ; ils excluent la capture et le
transport. La géométrie et les programmes sont mis en cache pendant le rendu.
Les mesures sont celles de la machine de test, pas une cadence garantie sur tout GPU.

Résultat du 27 septembre 2026 : **575 assertions passent**, avec les références
WebGL complémentaires. Sur une GeForce RTX 3080 Ti, Qt 6.8.3, build MSVC `/O2` :

| Cas synthétique à 800 × 600 | Prepare moyen | Upload + GPU + readback moyen | p95 frame |
| --- | ---: | ---: | ---: |
| Classic inset | 0,023 ms | 6,230 ms | 7,431 ms |
| Soft plein écran | 0,020 ms | 6,523 ms | 8,055 ms |
| Contours crop HQ | 32,010 ms | 6,491 ms | 7,798 ms |

`Prepare` Contours est exécuté sur le worker lors d'un nouvel état de géométrie,
et mis en cache. Il ne doit pas être ajouté aux temps de chaque frame. Les mesures
excluent aussi la compilation initiale des shaders, amortie avant les 24 frames.
