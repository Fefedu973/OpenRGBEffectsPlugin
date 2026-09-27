# Validation — 2026-09-27

MSVC x64 / Qt 6.8.3 / Windows, branche intelligence-prototype sur base ab233a5.
Aucun accès aux contrôleurs physiques, aucune capture personnelle activée.

| Test | Résultat effectivement obtenu |
|---|---|
| VideoEngine C++ | 462 assertions réussies |
| Shader vidéo, vrais ShaderProgram/ShaderPass | 60 checks, 27 comparaisons image entière CPU/GPU ; écart maximal 1/255 sRGB |
| MusicDirector C++ | 769 assertions synthétiques réussies |
| Shader musique, vrais ShaderProgram/ShaderPass | 19 checks, 13 comparaisons image entière ; écart maximal 1 niveau RGB8 linéaire |
| DLL Effects complète dans un hôte API5 vide, avec ONNX Runtime CPU | 790 checks réussis |
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
Candidate de la validation ONNX/UI :
`83DF29983C6BB9596A4269CFCB24257495EAE19B2257A1CF43A2244FFB56CC00`.
Logs locaux : `build-intelligence-inference.log` et
`build/intelligent-ambience-ui/model-ui-run.log`.
Les validations détaillées du backend et ses limites sont dans
[inference-tests.md](inference-tests.md). Le manifeste de déploiement conserve
le hash exact de la version finalement installée.
WebPage reste compilé avec le SDK WebView2 existant ; IntelligentAmbience ne
l'utilise pas. Les avertissements du build final concernent la conversion
size_t/unsigned du RhythmTracker existant.

Le binaire quotidien n'a pas été remplacé : son hash reste
`ECC0899DD74683CA086F879E5DD2416C1F9670C75ADD33715541A7FA32CE71A3`.
Ces résultats ne valident pas encore la qualité sur contenus réels, la
calibration du rectangle, la performance du layout complet ou un modèle IA.

Les runners Python de ce dossier sont uniquement des outils de compilation/test
à lancer dans un environnement MSVC x64. Ils ne sont pas requis dans OpenRGB.
