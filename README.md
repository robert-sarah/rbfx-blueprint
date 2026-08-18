# rbfx-blueprint

**rbfx-blueprint** est un moteur et framework de jeu **C++17 pour la 2D et la 3D**, basé sur le fork [rbfx](https://github.com/rbfx/rbfx) de [Urho3D](https://github.com/urho3d/Urho3D). Le projet conserve le contrôle offert par un moteur code-first tout en ajoutant une chaîne de production intégrée : éditeur extensible, graphes visuels Blueprint, langage gameplay rbscript, outils de rendu et de contenu, diagnostics corrélés et orchestration sémantique par **World Fabric**.

> **État du projet :** branche active de développement. Les fondations P0, la majorité des fonctionnalités P1, les panneaux P2 principaux et les premières fondations P3 sont présentes dans le dépôt. Le projet n’est pas présenté comme une version finale certifiée pour toutes les plateformes.

## Vision

L’objectif de rbfx-blueprint est de réunir dans un même moteur les trois modes de production suivants :

| Couche | Rôle | Intégration |
| --- | --- | --- |
| **C++** | Runtime, rendu, physique, réseau, outils et extensions natives | API de réflexion rbfx partagée avec les autres couches |
| **Blueprint** | Graphes visuels pour gameplay, logique, outils et production | Nœuds réfléchis, sous-graphes, commentaires, recherche et édition dans l’éditeur |
| **rbscript** | Langage gameplay typé à syntaxe avec accolades | Conçu pour exploiter la même réflexion rbfx que C++ et Blueprint |
| **World Fabric** | Graphe sémantique transversal | Relie ressources, dépendances, build, simulation, profiling, réseau et production |

## World Fabric : la différenciation du projet

**World Fabric** est le graphe de dépendances sémantiques du moteur. Il ne représente pas uniquement des fichiers : chaque nœud peut décrire une scène, un asset, un shader, un graphe Blueprint, un script, une tâche de build, une simulation ou un système runtime. Les dépendances sont persistées dans des ressources JSON et peuvent être analysées, ordonnées, profilées et interrogées depuis l’éditeur.

Cette architecture permet de relier la cause et l’effet dans un pipeline de production : une modification de ressource peut être suivie vers ses consommateurs, ses tâches invalidées, ses événements temporels et ses coûts CPU/GPU. Les services runtime associés incluent l’analyse d’impact transitive, les requêtes sémantiques, le profiling corrélé, la simulation déterministe et les opérations de collaboration versionnées.

## Fonctionnalités disponibles

### P0 — Fondations de l’éditeur

Le socle P0 met en place une expérience d’éditeur cohérente : système visuel partagé, docking, workspaces, palette de commandes, autosave et récupération, filtres dans l’Asset Browser, l’Inspector et l’Outliner, contrats UI communs et tests automatisés associés.

### P1 — Pipeline de production intégré

Les éditeurs et ressources suivants sont disponibles dans la branche `blueprint-foundation` :

| Domaine | Fonctionnalités |
| --- | --- |
| **Rendu** | Ressource et éditeur **Shader Graph** |
| **Effets** | Ressource et éditeur **VFX Graph** |
| **Audio** | Ressource et éditeur **Audio Mixer** |
| **Animation et cinématique** | Ressource et éditeur **Sequencer** |
| **Construction** | **Build Dashboard** et ressource de build déterministe |
| **Monde** | Inspecteur de production Terrain, TileMap2D et NavigationMesh |
| **Multijoueur** | **Multiplayer Profile**, réglages de démarrage, réplication et diagnostics |
| **Routage des ressources** | Extensions dédiées enregistrées dans `StandardFileTypes` |

Ces composants fournissent les contrats d’édition, la persistance JSON, les validations et les points d’intégration nécessaires à une chaîne de production plus large. Ils ne remplacent pas encore une matrice de certification complète pour Windows, macOS, Linux, mobile, WebAssembly et consoles.

### P2 — World Fabric et production corrélée

La couche P2 comprend :

| Service | Capacités |
| --- | --- |
| **Dependency Explorer** | Édition des nœuds et arêtes, inspecteur et ordre de build |
| **Impact Analysis** | Propagation transitive des impacts dans le graphe |
| **Semantic Query** | Recherche de nœuds selon leurs types, tags et métadonnées |
| **Correlated Profiler** | Corrélation des statistiques de nœuds avec les mesures runtime |
| **Semantic Timeline** | Association de nœuds World Fabric aux événements temporels de Sequencer |
| **Incremental Scheduler** | Visualisation de l’invalidation et de l’état des tâches par nœud |
| **Deterministic Reproduction** | Démarrage, restauration, replay et historique de snapshots |
| **Collaboration** | Clients connus, verrouillage, opérations, révisions et diagnostics de synchronisation |

Les panneaux P2 sont intégrés à `WorldFabricTab` et s’appuient sur les services runtime de `Source/Urho3D/WorldFabric/`, plutôt que sur des données d’interface isolées.

### P3 — Écosystème et fondations extensibles

La couche P3 livrée étend World Fabric au-delà de l’éditeur et fournit les contrats nécessaires à une production distribuée :

| Service | Capacités livrées |
| --- | --- |
| **PluginRegistry et PluginSDK** | Manifestes versionnés, capacités, dépendances, détection de cycles, ordre d’activation déterministe, version ABI, callbacks de réflexion/Blueprint/rbscript/éditeur et digest reproductible |
| **DistributedPackageRegistry** | Registre de packages, résolution sémantique des versions, plans de réplication, remplacements contrôlés, persistance JSON et digest stable |
| **WorldFabricRealtimeSession** | Présence multi-utilisateur, horloge de Lamport, enveloppes ordonnées, accusés de réception et suivi des clients actifs |
| **RbScriptLspService** | Protocole LSP/JSON-RPC, ouverture et mise à jour de documents, diagnostics, complétion, hover, définition, renommage et intégration avec la réflexion rbfx |
| **InteractiveDocumentation** | Index de pages et symboles, recherche, rendu Markdown/HTML, import du registre de types et sérialisation JSON |
| **IncrementalScheduler** | Invalidation transitive, propagation des dépendances, détection de cycles, tâches prêtes et digest déterministe |
| **ContentAddressedCache** | Cache d’artefacts adressés par contenu, digests SHA-256/FNV-1a, révisions et remplacement versionné |
| **GameplayTestHarness** | Exécution déterministe de callbacks, ordre stable, seed, limites de frames, résultats, digest et correction de l’ABI des utilitaires de tests |
| **HotReloadStateStore** | Capture/restauration de champs runtime, générations de hot reload, validation, suppression et digest stable |
| **CI native** | Job `blueprint-native-validation` couvrant Linux, Windows MSVC x64 et macOS arm64/x64 dans le workflow GitHub Actions |

La validation locale de cette livraison a été effectuée sur une reconstruction propre Linux : **333/333 tests CTest passent**. La matrice CI prépare les validations natives Windows et macOS ; elle ne remplace pas encore un smoke test graphique exécuté sur chaque système.

### Extensions uniques de production

Les trois services suivants prolongent World Fabric au-delà d’un simple graphe de dépendances et sont intégrés dans `WorldFabricTab` :

| Extension | Fonction professionnelle |
| --- | --- |
| **Causal World Fabric Debugger** | Capture des preuves causales séquencées, analyse des chaînes de dépendances, résumé de cause, calcul des nœuds impactés et diagnostic manuel depuis un nœud sélectionné. |
| **Universal Deterministic Time Machine** | Historique borné multi-domaine, états et entrées par frame, restauration, replay, branches d’investigation, comparaison de frames et recherche de la première divergence entre branches. |
| **Semantic Build Capsule** | Capsule JSON canonique regroupant environnement, digests World Fabric/Time Machine, entrées sémantiques et plugins, avec validation, empreinte déterministe et diff entre builds. |

Ces services sont conçus comme des contrats runtime réutilisables par l’éditeur, la CI, le profiler, le réseau et les outils de support. Ils fournissent une base de traçabilité et de reproduction ; ils ne constituent pas encore à eux seuls une certification de production ou une capture automatique complète de tous les systèmes du moteur.

## Architecture du dépôt

```text
Source/
├── Urho3D/
│   ├── Blueprint/       Runtime et réflexion des graphes visuels
│   ├── RbScript/        Langage et compilation rbscript
│   ├── WorldFabric/     Graphe sémantique, simulation, profiling et plugins
│   ├── Graphics/        Rendu, RenderGraph et ressources graphiques
│   ├── Network/         Runtime réseau et profils multijoueur
│   └── ...              Sous-systèmes C++ rbfx/Urho3D
├── Editor/
│   ├── Foundation/      Onglets et contrats de l’éditeur de production
│   └── ...              Applications et extensions de l’éditeur
└── Tests/               Tests Catch2 v3 des ressources et services
```

Les fichiers C++ des répertoires principaux sont découverts automatiquement par la configuration CMake existante. Les ressources de production utilisent les conventions rbfx de chargement, sauvegarde JSON, réflexion et validation.

## Prérequis

Le développement principal est réalisé avec **C++17**, **CMake**, **Ninja** et un compilateur compatible GCC 13 ou équivalent. Sous Linux, les dépendances graphiques et système usuelles d’OpenGL, Vulkan, X11, DBus et des bibliothèques incluses doivent être disponibles. Les dépendances tierces du moteur sont gérées par le dépôt et sa configuration CMake.

## Compilation et tests sous Linux

Depuis la racine du dépôt :

```bash
cmake -S . -B build -G Ninja \
  -DURHO3D_TESTING=ON \
  -DURHO3D_EDITOR=OFF \
  -DURHO3D_PLAYER=OFF \
  -DURHO3D_CSHARP=OFF

cmake --build build --target Tests -j2
ctest --test-dir build --output-on-failure
```

Pour compiler l’éditeur Linux en mode Debug :

```bash
cmake -S . -B build-editor -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DURHO3D_EDITOR=ON \
  -DURHO3D_PLAYER=OFF \
  -DURHO3D_TOOLS=OFF \
  -DURHO3D_TESTING=OFF \
  -DURHO3D_CSHARP=OFF

cmake --build build-editor --target Editor -j2
```

Le build Editor Linux validé dans cette branche produit `build-editor/bin/Debug/Editor`. Le build de test produit le binaire Catch2 dans `build/bin/`, selon la configuration choisie par CMake.

## Portabilité

La base rbfx vise les environnements desktop et dispose d’une architecture CMake portable. La validation complète doit toutefois être distinguée de la simple capacité théorique de compilation :

| Plateforme | Situation documentée dans cette branche |
| --- | --- |
| **Linux x86_64** | Configuration et compilation de l’éditeur validées dans l’environnement de développement |
| **Windows** | Configuration à valider par smoke test graphique réel et exécution native |
| **macOS** | Configuration et smoke tests à compléter sur environnement macOS natif |
| **Android, iOS, WebAssembly, consoles** | Adaptateurs et matrices de validation à poursuivre selon les toolchains disponibles |

Les contributions qui ajoutent une plateforme doivent fournir une commande de configuration, un build reproductible et, lorsque l’interface est concernée, un smoke test natif.

## Contribuer

Les contributions doivent rester compatibles avec les conventions C++17 et les conteneurs rbfx/EASTL utilisés par le projet. Toute nouvelle ressource doit définir une persistance stable, une validation négative, un digest lorsque cela est pertinent et un test Catch2. Toute extension d’éditeur doit respecter les contrats `ResourceEditorTab`, utiliser les mécanismes d’annulation existants et éviter de faire transiter des pointeurs const vers les champs ImGui mutables.

Avant de créer une pull request, exécutez au minimum `git diff --check`, la suite `Tests` et le build Editor si vos modifications concernent `Source/Editor/`. Les artefacts tels que `build/` et `build-editor/` ne doivent pas être commités.

## Licence et provenance

rbfx-blueprint est distribué sous la licence MIT. Le projet est dérivé de rbfx et d’Urho3D ; les notices de copyright amont sont conservées dans [LICENSE](LICENSE). Les contributions spécifiques à rbfx-blueprint sont attribuées aux auteurs et contributeurs du fork.

## Liens

| Ressource | Lien |
| --- | --- |
| Dépôt rbfx-blueprint | [github.com/robert-sarah/rbfx-blueprint](https://github.com/robert-sarah/rbfx-blueprint) |
| Branche de développement actuelle | [`blueprint-foundation`](https://github.com/robert-sarah/rbfx-blueprint/tree/blueprint-foundation) |
| Projet amont rbfx | [github.com/rbfx/rbfx](https://github.com/rbfx/rbfx) |
| Projet amont Urho3D | [github.com/urho3d/Urho3D](https://github.com/urho3d/Urho3D) |
| Licence | [LICENSE](LICENSE) |

## État de maturité

rbfx-blueprint dispose maintenant d’un socle d’éditeur et de production nettement plus large qu’un prototype minimal : les ressources sont persistées, les contrats d’interface sont testés, World Fabric fournit une couche sémantique transversale, l’écosystème P3 est présent et **333/333 tests Linux passent** dans cette session. Une qualification « production industrielle » complète nécessite encore des tests de charge, des projets de référence, l’exécution des validations natives Windows/macOS, une documentation utilisateur plus étendue et des campagnes de stabilité longue durée.
