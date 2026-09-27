# Apparence Better en rendu GPU natif

La fonction `better_capture::Prepare` transforme les métadonnées
`better.native-rendering` v1 en graphe GLSL. Il ne capture rien, ne contacte aucun
service et n'exécute aucun JavaScript. Le fournisseur doit auparavant avoir apparié
la frame brute, la couverture et l'état de rendu de la même génération.

Le contrat est [Appearance.h](Appearance.h). Le moteur consomme les textures externes
`raw`, `coverage` et, pour Contours, `geometry`. Les pixels externes ont leur origine
en haut à gauche. Les sorties FBO utilisent l'origine OpenGL ; `bInputFlip` distingue
explicitement les deux orientations. Les passes sont en RGBA16F prémultiplié, sans
blending, avec un seul readback final. Le graphe est borné à 32 passes et 128 Mio
d'intermédiaires. Le résultat final est opaque sur fond noir.

La préparation est à mettre en cache par état d'apparence/géométrie et dimensions,
pas à refaire par image. Le masque de Contours est limité à 320 × 320. Les matrices
et coordonnées sont finies et bornées ; 128 sources au maximum sont acceptées.
Une entrée invalide ou un dépassement de budget échoue explicitement.

## Réglages repris

| Réglage Better | Domaine et comportement |
| --- | --- |
| `screenX/Y`, `screenWidth/Height` | Placement logique dans 320 × 200 ; dimensions 1…320 et 1…200, position bornée au canvas. |
| `pictureMode` | Standard, Cinema, Mono, Vivid, Dominant, HD ; chaîne de filtres CSS Better, distincte des modes SignalRGB. |
| `hue`, `brightness`, `saturation` | −180…180 degrés ; −100…100 pour les deux facteurs. |
| `blur`, `interpolation` | Flou d'image de 1 pixel logique ; lissage ou échantillonnage pixelated. |
| `ambilight`, `ambilightStyle` | Halo activé ; Classic, Soft ou Contours. |
| `ambilightBlur/Spread` | 0…100 pixels logiques. Classic : dilatation puis flou. Soft : deux flous pondérés 0,65/0,35. |
| `ambilightSaturation/Intensity/Cutoff` | 0…10, 0…200 %, 0…100 %. Cutoff nul désactive le seuil. |
| `ambilightEdgeDepth/Mix/Reach/Fade` | 1…20 %, 0…30 %, 1…200 pixels logiques, 0…100 %. |
| `ambilightFullscreen`, `hideSources` | Le masquage de l'image est effectif seulement avec halo plein écran. |

`enabled`, `webEnabled` et la cadence du serveur ne sont pas des réglages
d'apparence de ce renderer : son activation et son transport sont indépendants.
Le ratio du canvas est étiré vers les dimensions de sortie, comme dans Better.

La source brute est le RGB prémultiplié sur noir, avec alpha de transport opaque.
La couverture fournit son vrai alpha. Les filtres déprémultiplient une fois,
appliquent la chaîne couleur, puis prémultiplient à nouveau. Un pixel noir couvert
reste donc une partie de la source.

Contours utilise la géométrie des sources, jamais leurs pixels noirs. Le masque
reproduit le clip rectangle, le clip de crop, puis le remplissage du renderer WEB :
leurs couvertures antialiasées se multiplient aux bords coïncidents. Les sources se
composent en union opaque, et la recherche des bords ne crée pas de frontière dans
leurs zones de recouvrement. Les bandes de couleur s'arrêtent au premier vide ;
elles ne peuvent pas franchir un trou pour prélever une autre source.

## Fidélité et limites

Les tests exécutent le **moteur GPU de production**, avec les 13 scènes synthétiques
de Better. La géométrie et l'image brute sont identiques pour les trois cas sans
filtre. L'erreur moyenne par canal est au plus 0,195/255 pour les filtres sans halo,
0,166 pour Classic, 0,359 pour Soft, 0,394 pour Contours HQ et 0,878 pour Contours
plein écran avec sources cachées, sur ces fixtures.

Ce n'est pas une promesse de pixels identiques. Qt et Chromium rasterisent quelques
pixels de contour différemment (7 pixels de classification sur 64 000 dans le cas
plein écran), ce qui déplace localement la projection du halo. Des écarts ponctuels
atteignent 170/255 sur cette référence, malgré l'erreur moyenne inférieure à 1.
Une seconde référence force le backend WebGL réel de Better : erreur moyenne
0,779 pour ce même cas. Les flous utilisent une approximation gaussienne séparable
bornée à 65 échantillons ; le flou de l'image est limité au canvas avant placement.
Ces différences sont documentées, pas masquées par les tests.

Voir [les tests et leurs seuils](../../tests/room-better-appearance/README.md).
Aucun écran réel, aucune source utilisateur ni contrôle matériel n'est utilisé.

## Provenance

Implémentation C++/GLSL originale sous GPL-2.0-or-later, suivant le contrat et les
formules de BetterSignalRGBScreenCapture, commit
`6979d38d1844bee549c6bb614901c9de7c9c44e4` :

- `docs/native-rendering-v1.md` ;
- `Services/WebOutput/StreamingCanvasPage.js` et `.html` ;
- `Services/WebOutput/ContourHalo.js` ;
- `tests/fixtures/native-rendering-v1` et `NativeRenderingReferenceTests.cjs`.

Ces chemins appartiennent au [dépôt Better](https://github.com/Fefedu973/Better-SignalRGB-Screen-Capture/tree/6979d38d1844bee549c6bb614901c9de7c9c44e4).
Les sources JS, images et fixtures sont lues dans un checkout distinct pendant les
tests ; elles ne sont pas recopiées dans ce module. Le README Better annonce MIT,
mais aucun fichier LICENSE/COPYING suivi n'a été trouvé à cette révision : cette
mention n'est donc pas présentée ici comme une licence vérifiée de ses fichiers.
