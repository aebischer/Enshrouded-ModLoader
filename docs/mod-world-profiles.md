Référence normative commune : [standard V1 du SDK](../sdk/STANDARD_V1.txt). Ce document complète le standard avec des notes de fonctionnement.

# Contrat de chargement 0.5

## Installation d'un mod

Placer le module dans `Mods/` avec un fichier voisin **nom.dll.mod** :

```ini
ENSHROUDED_PACKAGE_V1
kind=mod
version=1.0
abi=1
require=monmod/support.dll@1.0
```

Les chemins `require` sont relatifs Ã  **Mods**, utilisent `/`, et demandent
une version exacte. La ligne require est facultative et peut Ãªtre rÃ©pÃ©tÃ©e
pour des dÃ©pendances distinctes. Les clÃ©s inconnues, doublons, versions/ABI
invalides et cycles sont refusÃ©s. La profondeur maximale est 64 ; 1024 modules
au total, 64 dÃ©pendances directes par module, manifeste de 16 Kio maximum.

Une bibliothÃ¨que utilise le mÃªme format avec `kind=library`. Elle ne doit pas
exporter EnshroudedModInitialize et n'est pas activable indÃ©pendamment. La page
indique si elle est requise. Un mod dÃ©pendant d'un autre **mod** exige que celui-ci
soit explicitement activÃ©. Une bibliothÃ¨que nÃ©cessaire est chargÃ©e automatiquement.

Le mod doit exporter avec liaison C :

```cpp
extern "C" __declspec(dllexport)
int EnshroudedModInitialize(const EnshroudedModContext* context);
```

Voir `src/mod_api.h`. VÃ©rifier size et abi_version ; retourner 1 en cas de succÃ¨s.
L'initialisation est appelÃ©e hors de DllMain, une seule fois pour ce module.
Le contexte et le GUID restent valides jusqu'Ã  la fermeture. Le mod doit gÃ©rer
ses propres ressources de partie : aucun pointeur vers un monde natif n'est
fourni. Ne pas lever d'exception au travers de cette ABI C. Une exception C++
propagÃ©e ou un refus d'initialisation bloque le dÃ©marrage et impose la relance.
Une violation mÃ©moire native peut toujours faire planter le processus.

## VÃ©rifications avant exÃ©cution

Le chargeur valide tout le graphe avant le premier LoadLibrary : manifests,
versions exactes, chemins, identitÃ© physique des fichiers, noms de DLL non
ambigus, PE x64, export attendu et imports ordinaires/diffÃ©rÃ©s dÃ©clarÃ©s.
Les dÃ©pendances systÃ¨me sont recherchÃ©es dans System32 ; les autres doivent
Ãªtre dÃ©clarÃ©es. Les fichiers validÃ©s restent ouverts sans partage d'Ã©criture
ni de suppression pendant le chargement. Les alias et liens physiques vers
une mÃªme DLL sont refusÃ©s avant initialisation. Un module dÃ©jÃ  chargÃ© hors
profil est refusÃ©, car son Ã©tat/version ne sont pas maÃ®trisÃ©s.

Cela ne contient pas du code malveillant : une DLL peut appeler LoadLibrary
elle-mÃªme. Le manifeste est un contrat de coopÃ©ration, pas une signature de
confiance. DLL, manifests et dossiers doivent provenir de sources de confiance.
Les droits d'Ã©criture sur le dossier du jeu restent une frontiÃ¨re de confiance.

## Profils et mondes

Chaque profil `ModLoader/worlds/<GUID>.mods` contient :

```text
ENSHROUDED_MODS_V1
monmod/principal.dll
```

UTF-8, BOM facultatif, LF et CRLF acceptÃ©s. Chemins normalisÃ©s, 1024 entrÃ©es
maximum de 1024 octets, doublons refusÃ©s. La lecture est bornÃ©e en taille avant
allocation et conversion. Un profil invalide ne devient pas une liste vide.

Depuis 0.5.2, un fichier absent représente une configuration vide pour **tout
monde**. Aucun bouton de confirmation ni suivi de création n’est nécessaire.
Activer un mod crée le fichier au premier enregistrement. Supprimer le profil
réinitialise donc les choix. Un fichier présent mais invalide continue de
signaler une erreur ; le supprimer puis Actualiser permet de repartir à zéro.
Les DLL déjà résidentes ne sont pas déchargées : relancer le jeu reste nécessaire
si la configuration demandée diffère de celle chargée.

Chaque modification utilise un verrou d'Ã©criture et compare la version lue
avec le fichier actuel. En cas de conflit, Actualiser relit les choix avant
un nouvel essai. Le temporaire est unique, vidÃ© sur disque puis publiÃ© par
remplacement. Un Ã©diteur externe qui ignore le verrou reste hors protocole :
fermer le jeu pour modifier/importer ces fichiers manuellement.

Copier **le profil portant le mÃªme GUID avec sa sauvegarde de jeu** lors d'un
transfert ou d'une restauration, ainsi que les versions de mods requises.
La synchronisation Steam Cloud des profils n’est pas implémentée. Un profil
omis lors du transfert équivaut, comme une suppression, à zéro mod activé.

## Sessions et interface

La liste distingue mods, bibliothÃ¨ques, fichiers manquants et manifests invalides.
Les versions et un identifiant abrÃ©gÃ© du monde sont affichÃ©s. Changer de page
sÃ©lectionne une ligne visible. Les mÃ©tadonnÃ©es disque sont mises en cache Ã 
l'ouverture/Actualiser ; le rendu n'interroge plus le systÃ¨me de fichiers.
Le catalogue est bornÃ© en nombre d'entrÃ©es/profondeur/temps d'Ã©numÃ©ration.

Une nouvelle liste aprÃ¨s un chargement ou un autre monde avec des DLL rÃ©sidentes
impose de quitter puis relancer le jeu. Revenir Ã  la liste dÃ©jÃ  chargÃ©e pour
le mÃªme GUID est permis. Les DLL restent rÃ©sidentes aprÃ¨s un Ã©chec partiel ;
la partie est bloquÃ©e. La voie de connexion distante interceptÃ©e est bloquÃ©e
si des mods locaux sont rÃ©sidents. Pas de dÃ©chargement Ã  chaud, protocole de
mods serveur, ni compatibilitÃ© automatique EML/Enshwick.

Le bootstrap compare le SHA-256 de l'exÃ©cutable et les cinq cibles de hooks en
mÃ©moire avec le fichier. Une erreur affiche une alerte Windows. La DLL se rend
rÃ©sidente et DllCanUnloadNow renvoie S_FALSE. Les journaux sont horodatÃ©s et
limitÃ©s par rotation Ã  environ 2 Mio plus le fichier prÃ©cÃ©dent.

Les tests techniques ne remplacent pas la validation en jeu des parcours
natifs (dÃ©marrage, retour menu, serveur, manette, rÃ©solutions). Le rendu natif
0.3 a Ã©tÃ© confirmÃ© auparavant ; le comportement 0.5 doit Ãªtre vÃ©rifiÃ© en jeu.

## Export et import accompagnant une sauvegarde

Jeu fermé, utiliser `tools/transfer-profile.ps1` avec `-Action Export` ou
`-Action Import`, `-GamePath`, `-WorldId` (GUID complet) et `-Path` (fichier
`.worldmods`). Le paquet transporte l'identité du monde, le profil et son
empreinte. L'import vérifie le GUID, les limites et l'intégrité avant écriture,
prend le verrou, puis remplace atomiquement le profil. L'ancien profil est
conservé en `.before-import`. Le paquet ne contient ni la sauvegarde du jeu ni
les DLL : transférer ces éléments et leurs versions ensemble.

Exemple :

```powershell
.\tools\transfer-profile.ps1 -Action Export -GamePath 'D:\SteamLibrary\steamapps\common\Enshrouded' -WorldId 'GUID_32_CARACTERES' -Path 'D:\Sauvegardes\monde.worldmods'
```
