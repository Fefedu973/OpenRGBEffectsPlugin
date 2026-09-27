# Validation — 2026-09-27

MSVC x64 / Qt 6.8.3 / Windows, branche intelligence-prototype sur base ab233a5.
Aucun accès aux contrôleurs physiques, aucune capture personnelle activée.

| Test | Résultat effectivement obtenu |
|---|---|
| VideoEngine C++ | 462 assertions réussies |
| Shader vidéo, vrais ShaderProgram/ShaderPass | 60 checks, 27 comparaisons image entière CPU/GPU ; écart maximal 1/255 sRGB |
| MusicDirector C++ | 769 assertions synthétiques réussies |
| Shader musique, vrais ShaderProgram/ShaderPass | 19 checks, 13 comparaisons image entière ; écart maximal 1 niveau RGB8 linéaire |
| DLL Effects complète dans un hôte API5 vide | 550 checks réussis |

Le test de DLL conserve les 35 presets historiques et vérifie la catégorie,
les contrôles du nouvel effet, les profils/calibration/scènes, les valeurs
malformées, le rejet des programmes shader fournis dans un profil, la vraie
animation GPU de démonstration, l'arrêt/reprise et un heartbeat de l'interface.
Le descripteur BSRGBSC de test pointe dans un dossier temporaire inexistant.
Le mode vidéo synthétique est le seul mode démarré dans cet hôte.

DLL candidate : `build/release/OpenRGBEffectsPlugin.dll`.
SHA-256 : `E521EE56431F850F7254D629DD83F197D92D001F16A6F2FF4607780F7005A953`.
Logs locaux : `build-intelligence-final.log` et
`build/intelligent-ambience-ui/ui-run.log`.
WebPage reste compilé avec le SDK WebView2 existant ; IntelligentAmbience ne
l'utilise pas. Les avertissements du build final concernent la conversion
size_t/unsigned du RhythmTracker existant.

Le binaire quotidien n'a pas été remplacé : son hash reste
`ECC0899DD74683CA086F879E5DD2416C1F9670C75ADD33715541A7FA32CE71A3`.
Ces résultats ne valident pas encore la qualité sur contenus réels, la
calibration du rectangle, la performance du layout complet ou un modèle IA.

Les runners Python de ce dossier sont uniquement des outils de compilation/test
à lancer dans un environnement MSVC x64. Ils ne sont pas requis dans OpenRGB.
