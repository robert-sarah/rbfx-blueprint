# Production Readiness

## Objectif

`ProductionReadiness` complète `ProductionFinalization` avec des critères de sortie vérifiables pour une version stable, documentée, reproductible et exploitable en production. Ces contrats ne remplacent pas l’exécution sur les machines cibles : ils rendent les preuves explicites et empêchent de déclarer une release comme certifiée lorsqu’une étape native manque.

> Une suite de tests réussie démontre le comportement des contrats testés. Elle ne certifie pas à elle seule un pilote GPU, un gestionnaire de fenêtres, un SDK propriétaire, une installation Windows/macOS ou un jeu livré.

## Contrats disponibles

| Contrat | Responsabilité |
| --- | --- |
| `ProductionDiagnosticLog` | Journal borné, sérialisable et redigé pour les chemins sensibles. |
| `ProductionSanitizerGate` | Vérification des preuves AddressSanitizer, UBSan, analyse statique et fuzzing. |
| `ProductionSoakGate` | Validation de durée, nombre de frames, temps de frame, mémoire, crashs et races. |
| `ProductionNativeReleaseGate` | Matrice explicite Linux, Windows et macOS avec compilation, tests, smoke graphique, installation et paquet. |
| `ReproducibleBuildVerifier` | Comparaison et empreinte déterministe des toolchains, dépendances, sources et artefacts. |
| `ProductionPluginRegistry` | Manifests versionnés, ABI, signatures et registre de plugins. |
| `ProductionDependencyAudit` | Inventaire de dépendances, licences, hashes et vulnérabilités connues. |
| `ReferenceProjectCatalog` | Preuve de création, ouverture, édition, sauvegarde, packaging et lancement des projets 2D, 3D et hybrides. |
| `ProductionDocumentationGate` | Vérification de la couverture documentaire minimale. |
| `ProductionReleaseGate` | Validation combinée du packaging, des checksums, des symboles, du changelog, du rollback et des signatures. |
| `ProductionReadinessEvaluator` | Agrégation des gates sans masquer les éléments bloquants. |

## Preuves attendues pour la version 1.0

La version desktop 1.0 doit déclarer explicitement ses plateformes supportées. La matrice recommandée est Windows x86_64, Linux x86_64 et macOS x86_64/arm64. Android, WebAssembly et consoles doivent avoir leurs propres toolchains et ne doivent pas être implicitement considérés comme certifiés par une validation desktop.

Chaque plateforme doit fournir une preuve comprenant l’architecture, le compilateur, le backend graphique, le commit source, l’empreinte de l’artefact, la configuration, la compilation, les tests, le smoke graphique, l’installation et la vérification du paquet. Une archive cross-compilée ou une vérification PE ne remplace pas une exécution graphique native sur la plateforme cible.

Les trois projets de référence requis sont `reference-2d`, `reference-3d` et `reference-hybrid`. Pour chacun, la chaîne complète doit être exécutée : création, ouverture, édition, sauvegarde, packaging et lancement. Les chemins et empreintes doivent être conservés dans les artefacts CI afin de permettre une vérification indépendante.

## Reproductibilité

Les presets CMake versionnés dans `CMakePresets.json` fournissent des points d’entrée stables pour les builds Linux de tests, éditeur et packaging. Une preuve de build reproductible doit également conserver les versions de CMake, Ninja, compilateur, toolchain, dépendances, commit source, digest des sources et digest de l’archive.

Exemple Linux tests Release :

```bash
cmake --preset linux-tests-release
cmake --build --preset linux-tests-release
ctest --preset linux-tests-release --output-on-failure
```

Exemple Linux package :

```bash
cmake --preset linux-release-package
cmake --build --preset linux-release-package
```

Les répertoires `build-*` ne doivent jamais être committés. Les archives de release doivent contenir un manifeste, un fichier de checksums, la provenance du commit, les symboles séparés lorsque disponibles et un changelog.

## Durcissement

La gate sanitizer doit être alimentée par des jobs dédiés lorsque les runners le permettent. La gate soak doit recevoir des mesures réelles provenant de l’éditeur, du streaming, du hot reload, des sauvegardes et du gameplay. Les résultats doivent signaler les crashs, races, ressources invalides, dépassements de mémoire et dépassements de temps de frame.

Les parseurs JSON, Blueprint, rbscript, ressources, sauvegardes et manifests doivent être testés avec des entrées tronquées, inconnues, dupliquées, cycliques et volontairement malformées. Une entrée invalide doit produire un diagnostic actionnable et ne doit pas provoquer de crash.

## Documentation et sécurité

Avant une release finale, la documentation doit couvrir l’installation Windows/Linux/macOS, le premier projet 2D/3D/hybride, l’éditeur, Blueprint, rbscript, World Fabric, les assets, le réseau, la sauvegarde, les plugins, la migration et le dépannage. Les pages doivent préciser la version, la plateforme, le niveau de maturité et les limites connues.

La distribution doit conserver un inventaire des dépendances, les licences, les hashes et les vulnérabilités connues. Les artefacts publiés doivent être signés lorsque l’infrastructure de signature est disponible. Les crash reports doivent être désactivés par défaut ou explicitement consentis et ne doivent pas exposer de chemins utilisateur ou de secrets.

## Gater une release

Une release ne doit être déclarée prête que si les résultats suivants sont tous disponibles :

| Gate | Preuve minimale |
| --- | --- |
| Code | Analyse statique, sanitizers et fuzzing sans défaut critique connu. |
| Tests | Tests unitaires, intégration, éditeur, packaging, réseau, sauvegarde et régression graphique. |
| Plateformes | Exécution native Windows, Linux et macOS sur des machines et pilotes réels. |
| Performance | Budgets CPU, GPU, mémoire, chargement et réseau respectés. |
| Reproductibilité | Rebuild propre et artefacts comparables. |
| Documentation | Installation, tutoriels, API, dépannage, migration et plugins. |
| Sécurité | SBOM, licences, audit CVE, signatures et procédure de vulnérabilité. |
| Production | Projets de référence 2D, 3D et hybride livrés de bout en bout. |
| Release | GitHub Release, archives, checksums, symboles, changelog et rollback. |

Les contrats et leurs tests sont implémentés dans `Source/Urho3D/WorldFabric/ProductionReadiness.*` et `Source/Tests/TestProductionFinalization.cpp`. La suite Linux actuelle contient 370 tests validés après l’intégration des nouveaux contrats ; les validations natives Windows/macOS doivent être confirmées par les runners et machines cibles correspondants.
