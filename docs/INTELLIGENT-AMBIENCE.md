# Intelligent Ambience — prototype natif dans Effects

État du 27 septembre 2026. Le choix confirmé est **un nouvel effet du plugin
Effects existant**, relié au canvas Visual Map existant. Il ne s'agit pas d'une
nouvelle application, d'un onglet OpenRGB séparé ou d'un serveur intermédiaire.
Le prototype de recherche `../OpenRGB-Intelligence-Plugin` est conservé, à la
demande de l'utilisateur. Les ports du catalogue SignalRGB restent en pause.

## Ce qui est implémenté

L'effet `IntelligentAmbience`, catégorie **Advanced**, propose trois modes :
vidéo, musique et hybride. Le rendu final est un shader GLSL natif. La capture,
les profils, la prévisualisation, l'audio et l'envoi du canvas réutilisent Effects.
Visual Map distribue cette image sur les positions de Full Scale ; aucun
placement de périphérique n'est modifié. Aucun système de placement 3D ajouté.

- **Vidéo :** `ScreenSourceSelection` existant sélectionne BSRGBSC, sa scène,
  une source locale ou la capture native. Le moteur lit `Latest()` : l'image
  brute, sans le rendu glow de `LatestAppearance()`. L'effet Screen Ambience
  existant garde son rendu fidèle aux réglages BSRGBSC ; ce nouvel effet applique
  sa propre extrapolation sur le flux brut, comme les autres effets dérivés.
- **Extrapolation :** analyse bornée de mouvements de luminance, événements
  colorés quittant les bords, trajectoire et décroissance limitées dans le temps
  et l'espace. Coupures et correspondances insuffisantes limitent la prédiction.
  La référence classique prolonge les bords. Ce n'est pas une reconstruction
  réelle du hors-champ, ni de la génération apprise.
- **Musique :** le `RhythmSnapshot` du moteur audio existant alimente une
  composition de palettes, nappes et accents. Une période minimale par ambiance
  et un budget d'accents évitent de traiter chaque note comme un flash. Le mode
  de comparaison montre huit bandes de niveau. Aucun second capturer audio.
- **Hybride :** addition bornée de la composition musicale à l'ambiance vidéo,
  calculée en lumière linéaire avant conversion sRGB finale.
- **Démo :** une source synthétique permet de tester le rendu sans capture
  écran/audio ni modèle. Elle est explicitement signalée dans l'interface.

Les réglages du mode, de la prédiction, de sa durée/force, de la contribution
musicale, de la source et de la scène sont sérialisés dans les profils Effects.
Le rectangle de l'écran se règle dans un repère canvas **320 × 200**. Les valeurs
initiales **80,55,160,90 sont illustratives**, pas une calibration de la chambre.
Ce rectangle est nécessaire pour distinguer image observée et périphérie ;
la géométrie physique d'un écran ne se déduit pas de ses deux zones RGB.

## Chemin des données et coûts bornés

`BSRGSC/capture existante → dernière image valide → VideoEngine C++ → deux
textures numériques → GLSL Effects → routeur de canvas → Visual Map → appareils`.

L'analyse consomme au plus 30 nouvelles images par seconde ; le rendu vise
60 Hz, configurable par le réglage FPS d'Effects. Ce sont des cadences demandées,
pas une mesure de performance finale avec tous les appareils.
L'entrée d'analyse est sous-échantillonnée au plus à 192 × 192 sans interpolation
en gamma. Le moteur réduit ensuite les échantillons en lumière linéaire sur une
grille d'au plus 64 × 64. Les petits détails peuvent être perdus : c'est un
compromis d'analyse, pas un traitement photoréaliste. Au plus 128 événements
occupent une texture 3 × 128 RGBA32F. Les pixels du canvas restent rendus sur GPU.

Une image statique peut renouveler son bail sans nouvelle observation : la
grille reste visible mais la mémoire des événements continue d'expirer. Une
nouvelle source, une discontinuité ou un changement de géométrie remet la mémoire
appropriée à zéro. Les données expirées produisent du noir. Les textures sont
immuables ; renouveler leur durée de validité n'impose pas un nouvel upload GPU.

L'audio utilise le temps monotone QPC du moteur existant ; le snapshot est lu
avant l'horloge du rendu. Les séquences sont dédupliquées et les données audio
de plus de 250 ms sont rejetées. Aucun temps absolu très grand n'est envoyé au
shader sous forme de float.

`DynamicShaderImage` ne fournit pas de timestamp de capture : le mouvement vidéo
utilise l'intervalle entre observations consommées. La latence capture→LED n'est
donc pas mesurée par ce prototype. Un réglage de force ou de durée ne redémarre
pas la source et ne réclame pas à nouveau la scène BSRGBSC.

## Limites et suite utile

**Aucun modèle IA ni poids téléchargé ou exécuté.** Ce prototype est la référence
procédurale permettant de vérifier le raccordement et de comparer ensuite un
modèle. Les scores de confiance sont heuristiques. Ni compréhension sémantique
des objets, ni reconnaissance fiable d'émotions/instruments n'est annoncée.

Les audits de modèles et le cahier des charges original sont conservés dans
`../OpenRGB-Intelligence-Plugin/docs/`. Le choix natif dans Effects remplace les
propositions d'application autonome/SDK/Python de ce dossier, conformément aux
instructions ultérieures de l'utilisateur.

Avant de juger l'effet en usage réel : calibrer le rectangle de l'écran, comparer
référence/prédiction avec plusieurs contenus BSRGBSC, écouter différentes musiques
et mesurer les temps de rendu avec le layout complet. Un modèle appris éventuel
doit ensuite démontrer un gain sur cette référence avec des poids/export/licence
identifiés. La perception musicale et la qualité du hors-champ ne sont pas
établies par des tests synthétiques.

## Validation reproductible

Sources de tests dans `tests/intelligent-ambience/` : directeur musical, parité
GPU vidéo/musique, vraie DLL Effects dans un hôte de test sans périphérique,
persistance/arrêt/reprise et conservation des 35 presets déjà livrés.
Voir [les résultats de validation](../tests/intelligent-ambience/VALIDATION.md)
pour les résultats effectivement obtenus.

Cette branche `intelligence-prototype` part de `ab233a5` pour ne pas mêler les
44 nouveaux ports encore en validation à l'installation quotidienne. Les tests
exécutables sont des outils de développement, jamais des services à démarrer.
