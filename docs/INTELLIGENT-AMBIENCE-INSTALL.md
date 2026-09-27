# Installer le prototype natif

Cette branche ajoute un effet au plugin Effects existant. Elle n'installe aucun
service, serveur Python ou tâche de démarrage. OpenRGB continue à démarrer avec
sa configuration habituelle. Les modèles sont désactivés par défaut.

## Préparer et contrôler

1. Compiler la DLL avec le même Qt/MSVC et le même fork OpenRGB que l'installation.
2. Exécuter les tests dans `tests/intelligent-ambience/`, dont `run_ui.py --ort`
   avec le runtime CPU officiel obtenu par `inference_fetch_ort.py`.
3. Facultatif : créer trois profils dans un dossier temporaire avec
   `tools/create-intelligent-ambience-profiles.py --config <configuration>
   --output <staging-profils>`. Cet outil reprend Full Scale, la scène principale
   BSRGBSC et le périphérique audio des profils existants ; il ne modifie rien en
   place. Le rectangle de l'écran est une valeur illustrative à calibrer.
4. Lancer `tools/install-intelligent-ambience.ps1 -Mode Stage` avec les chemins
   absolus `-SourceDll`, `-RuntimeRoot`, `-StageDirectory` et, facultativement,
   `-ProfileRoot`. Stage contrôle les hashes, les DLL x64 et prépare le paquet.

Les sauvegardes de configuration peuvent contenir des données privées. Conserver
le dossier de staging dans `build`/`private`, sans le publier sur GitHub.

## Appliquer

Quitter normalement OpenRGB pour qu'il sauvegarde sa session et libère la DLL.
Lancer le même script avec `-Mode Apply -StageDirectory <stage> -InstallRoot
<dist-room> -ConfigRoot <configuration>`. Il refuse d'agir si OpenRGB tourne,
sauvegarde les fichiers et la configuration, puis installe le plugin, les deux
DLL ONNX, leurs licences et les trois profils facultatifs.

La dernière session, Full Scale, les paramètres et les anciens profils restent
en place ; les nouveaux profils ne sont pas activés automatiquement. Relancer
OpenRGB par son lanceur habituel. L'effet apparaît dans **Advanced → Intelligent
Ambience (prototype)** ; les profils ajoutés s'appellent **IA - Video**,
**IA - Musique** et **IA - Hybride**.

## Charger un modèle et revenir en arrière

Dans l'effet, choisir le manifeste du paquet vidéo ou musical et cocher son
activation. Le statut précise chargement, inférence, attente de données ou erreur.
Le contrat exact et les limites figurent dans
[Inference/README.md](../Effects/IntelligentAmbience/Inference/README.md).
Un modèle entraîné compatible et validé reste nécessaire pour obtenir un rendu
génératif appris ; les fixtures des tests ne sont pas des modèles artistiques.

Pour revenir à la DLL précédente : fermer OpenRGB puis appeler l'installeur avec
`-Mode Rollback`, les mêmes dossiers et le `-BackupDirectory` retourné à
l'installation. Le retour arrière refuse d'écraser les fichiers modifiés depuis.
Il ne remplace pas la dernière session ou les réglages par une ancienne copie.
