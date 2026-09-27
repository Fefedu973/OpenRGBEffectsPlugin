# Validation — 2026-09-27

MSVC x64 / Qt 6.8.3 / Windows, branche intelligence-prototype sur base ab233a5.
Aucun accès aux contrôleurs physiques, aucune capture personnelle activée.

| Test | Résultat effectivement obtenu |
|---|---|
| VideoEngine C++ | 462 assertions réussies |
| Shader vidéo, vrais ShaderProgram/ShaderPass | 60 checks, 27 comparaisons image entière CPU/GPU ; écart maximal 1/255 sRGB |
| MusicDirector C++ | 769 assertions synthétiques réussies |
| Shader musique, vrais ShaderProgram/ShaderPass | 19 checks, 13 comparaisons image entière ; écart maximal 1 niveau RGB8 linéaire |
| DLL Effects complète dans un hôte API5 vide, avec ONNX Runtime CPU | 729 checks réussis sur la candidate finale |
| Préparation modèles vidéo/PCM + champ de sortie | 154 checks réussis |
| Exécuteur natif ONNX Runtime CPU 1.30.0 | 109 checks réels réussis |
| Fenêtre PCM de la capture Windows existante | 2072 checks + 23 formats + 3340 régressions capture réussis |

Le test de DLL conserve les 35 presets historiques et vérifie la catégorie,
les contrôles du nouvel effet, les profils/calibration/scènes, les valeurs
malformées, le rejet des programmes shader fournis dans un profil, la vraie
animation GPU de démonstration, l'arrêt/reprise et un heartbeat de l'interface.
Le descripteur BSRGBSC de test pointe dans un dossier temporaire inexistant.
Les modes vidéo et musique synthétiques sont démarrés dans cet hôte. Les petits
graphes ONNX locaux produisent un champ rouge vidéo et vert musique : le test
lit le résultat final de la vraie prévisualisation GPU. Il vérifie aussi le
rechargement, la désactivation/réactivation, arrêt/reprise, persistance des
manifests et repli procédural sur manifeste invalide. Ces graphes sont des
fixtures mathématiques, pas des poids entraînés. Aucune capture audio réelle
ou nouvelle application externe n'est utilisée.

DLL candidate : `build/release/OpenRGBEffectsPlugin.dll`.
Candidate finale, code `3770c91`, metadata `1.0+ (git38)` :
`2274D08C2BCFA9571B4BFC33CB32DC1E53FBCDD74FD93B18966A1CB4F6A37DA4`.
Logs locaux : `build-intelligence-inference.log` et
`build/intelligent-ambience-ui/final-model-ui-run.log`.
Le compte UI inclut des vérifications répétées d'images pendant les attentes et
varie avec le nombre de frames ; il ne représente pas 729 scénarios distincts.
La première candidate avait passé 790 checks avec les mêmes scénarios.
Les validations détaillées du backend et ses limites sont dans
[inference-tests.md](inference-tests.md). Le manifeste de déploiement conserve
le hash exact de la version finalement installée.
WebPage reste compilé avec le SDK WebView2 existant ; IntelligentAmbience ne
l'utilise pas. Les avertissements du build final concernent la conversion
size_t/unsigned du RhythmTracker existant.

Installation effectuée le 27 septembre 2026 : DLL finale `2274D08C...F6A37DA4`,
runtime CPU et licences installés, trois profils ajoutés. Les 57 fichiers de
configuration préexistants ont conservé leur hash lors de cette installation.
Les 109 tests d'inférence ont aussi réussi contre le runtime du dossier installé.
Après relance, OpenRGB enregistre 95 effets et restaure Full Scale / Neon Shift ;
le SDK7 confirme les profils IA - Video, IA - Musique et IA - Hybride.

Une correction de configuration distincte, demandée ensuite par l'utilisateur,
remplace l'ancienne clé ignorée `UserInterface.MinimizeOnClose:false` par la clé
native `minimize_on_close:true`. Copie originale conservée avant modification.
L'application est relancée avec cette préférence ; le clic physique de fermeture
n'a pas été automatisé. Un crash de fin de processus a été observé après la
première fermeture ; des crashs OpenRGB existaient avant cette installation,
ce qui ne prouve pas qu'ils partagent la même cause. Ce défaut de sortie reste
à isoler, indépendamment du réglage de maintien dans la zone de notification.

Le journal de version du plugin conserve un ancien libellé compilé ; le hash
exact ci-dessus et le commit de code sont les références de cette livraison.
Ces résultats ne valident pas encore la qualité des modèles entraînés, la
calibration du rectangle ou la performance du layout complet avec un réseau réel.

Les runners Python de ce dossier sont uniquement des outils de compilation/test
à lancer dans un environnement MSVC x64. Ils ne sont pas requis dans OpenRGB.
