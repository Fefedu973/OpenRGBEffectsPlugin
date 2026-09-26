# Capture Windows : DXGI avec lecture CPU explicite

`WindowsScreenCapturer` conserve l'API `OnImage(const QImage&)`. Le backend normal utilise **DXGI Desktop Duplication**, puis copie les pixels d'une texture D3D11 de staging vers une `QImage` possédée. Ce chemin comporte bien une lecture GPU → CPU ; ce n'est pas du zéro copie. L'effet Ambient effectue ensuite sa normalisation vers son canvas de travail, par défaut 800 × 600.

## Cycle de vie et ressources

- `SetScreen` lit seulement le nom du `QScreen` sur le thread GUI. Un appel provenant d'un autre thread est reporté vers ce thread. Le worker reçoit une chaîne indépendante ; il ne consulte aucun `QScreen` ou `QPixmap`.
- Le nom est comparé au `DeviceName` des sorties DXGI. Le périphérique D3D11 est créé sur l'adaptateur de la sortie correspondante : l'écran choisi peut appartenir à un autre GPU que le moniteur principal.
- Device, contexte, duplication et staging sont conservés entre les frames. Le staging est recréé si ses dimensions changent. Une perte de duplication/device ou un échec de lecture ferme les ressources natives, puis provoque une nouvelle ouverture après 5 secondes. Une nouvelle sélection d'écran réinitialise immédiatement ce délai.
- `AcquireNextFrame` attend au plus 16 ms. Chaque acquisition réussie possède un garde `ReleaseFrame`, y compris sur les chemins d'erreur. `Map` utilise `DO_NOT_WAIT` avec une fenêtre de polling de 4 ms ; une seule copie reste en attente. Après une seconde sans disponibilité, la récupération habituelle est déclenchée.
- Les images utilisent un pool partagé de trois `QImage`. Un buffer encore retenu par le consommateur n'est jamais réécrit. Si les trois sont retenus, la frame est abandonnée. La cadence et le drapeau d'arrêt sont atomiques ; l'attente entre frames est réveillable et `Stop` rejoint le worker avant destruction.
- Le callback direct `OnImage` doit rester bref et ne doit pas appeler `Stop` ou détruire le capturer. Ambient utilise une connexion directe avec contexte, normalise la frame, puis remplace son image courante immutable. Le capturer n'émet pas de nouvelle image lors d'un timeout sur un bureau inchangé ; le heartbeat FrameSurface appartient à Ambient.

Le format BGRA, les pas de ligne source/destination et les rotations 0°, 90°, 180° et 270° sont traités explicitement. L'alpha de sortie est opaque. Les coordonnées DXGI/GDI sont des pixels natifs ; aucune origine Qt logique n'est multipliée par un DPI global.

## Repli GDI et limites

Si DXGI est indisponible, le repli utilise un DC propre à l'écran, un DC mémoire et un DIB top-down persistants, sans `QPixmap`. Sa cadence est plafonnée à 15 fps. Si l'écran est indisponible, les tentatives GDI sont espacées de 200 ms ; l'ouverture DXGI reste espacée de 5 secondes. Le changement de DPI est limité au thread de capture et restauré à sa sortie.

La capture reste une image SDR 8 bits par canal. Aucun tone mapping HDR/scRGB ni composition supplémentaire du curseur matériel n'est implémenté. Les restrictions de session distante, bureau sécurisé, contenu protégé, pilote ou droits Windows peuvent empêcher la capture. Le délai d'acquisition est borné, mais les appels pilote/GDI de création, transfert ou destruction ne constituent pas une garantie temps réel stricte.

**Aucune capture d'écran réelle n'a été exécutée pour cette validation.** Les rotations effectives d'un écran physique, les transitions de résolution/DPI, la récupération après perte de device, la qualité HDR et le coût CPU/GPU restent à mesurer sur le matériel avec autorisation.

## Tests sans écran

Depuis la racine du dépôt, avec un kit Qt/MSVC déjà installé :

```powershell
python tests/room-capture/run.py --qt C:\chemin\Qt\6.8.3\msvc2022_64
```

Le runner accepte `--vcvars` si Visual Studio Build Tools se trouve ailleurs. Il ne télécharge et n'installe rien. Il compile **le vrai fichier WindowsScreenCapturer.cpp**, génère le moc de la classe de base, puis lie Qt Core/Gui et D3D11/DXGI/GDI/User32. Le projet principal doit également lier `d3d11` et `dxgi` sous Windows.

Validation locale : **MSVC 14.44 et Qt 6.8.3, compilation et tests réussis**. Les tests couvrent :

1. Pixels synthétiques distincts, quatre rotations, padding de lignes et sentinelles, alpha et dimensions invalides.
2. `ReleaseFrame` exactement une fois après sortie normale, libération explicite et exception simulée de staging.
3. Repli, reprise après 5 secondes, changement de cible et bornes de cadence, sans attente réelle.
4. Propriété des images, saturation du pool, réutilisation de buffer et resize sans altérer les images retenues.
5. Vingt cycles de démarrage/arrêt avec modification concurrente de la cadence. Le test utilise uniquement `QCoreApplication` et ne sélectionne jamais d'écran : aucune sortie n'est énumérée, ouverte, dupliquée ou lue.

Les tests de politique et de garde emploient des données/objets factices ; ils ne simulent pas tous les appels COM ni un pilote DXGI.

## Sources primaires

- [Microsoft — AcquireNextFrame : timeout et perte d'accès](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgioutputduplication-acquirenextframe).
- [Microsoft — Desktop Duplication : orientation des surfaces](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/desktop-dup-api).
- [Microsoft — Map et DO_NOT_WAIT](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-map).
- [Microsoft — exemple de compensation de rotation](https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/DXGIDesktopDuplication/cpp/DisplayManager.cpp). La convention de rotation a été vérifiée contre cet exemple ; aucune portion de son code n'a été copiée.
