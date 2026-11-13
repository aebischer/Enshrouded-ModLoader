Référence normative commune : [standard V1 du SDK](../sdk/STANDARD_V1.txt). Ce document complète le standard avec des notes de fonctionnement.

# Paramètres des mods — 0.6.2

Le manifeste `.dll.mod` déclare le schéma, sans exécuter de code dans le menu :

```ini
setting=messages|bool|Messages de test|true
setting=effects|bool|Effets de test|false
setting=intensity|percent|Intensité de test|100|0|200|25
setting=mode|choice|Mode de test|Normal|Calme|Normal|Rapide
```

Ordre : clé, type, libellé, défaut, puis choix autorisés pour `choice`.
Les clés sont uniques, en minuscules/chiffres/underscore (40 caractères max).
Maximum 16 paramètres, 2 à 8 choix distincts pour une liste, 100 octets par
champ ; booléens stricts `true`/`false`. UTF-8 invalide et schémas incohérents
sont refusés. Les bibliothèques ne déclarent pas de paramètres.

Depuis 0.6.1, le panneau emploie les composants natifs de difficulté : ligne
sélectionnable, surbrillance, libellé à gauche, valeur centrée entre les flèches.
Les booléens affichent Désactivé/Activé. Les autres choix utilisent la même
ligne, avec flèches masquées aux extrémités. La zone de défilement native remplace
la pagination des paramètres et reprend la barre dorée du jeu. Les valeurs
sont enregistrées à chaque changement ; Actualiser relit les fichiers.

Chaque couple GUID de monde / identifiant de mod a son propre fichier :
`ModLoader/worlds/<GUID>.settings/<chemin-du-mod.dll>.ini`.
Fichier absent = valeurs par défaut ; un fichier présent mais invalide bloque
le chargement au lieu d’être ignoré. Lecture limitée à 16 Kio, validation des
valeurs, verrou exclusif, comparaison avec la dernière version lue, temporaire
unique vidé sur disque puis remplacement atomique. Les paramètres persistent
lorsque le mod est désactivé. Supprimer son fichier puis Actualiser restaure
ses défauts. Le profil d’activation `.mods` est indépendant.

Les mods déclarant des paramètres doivent exporter
`int EnshroudedModConfigure(const char* world_id, const char* settings_utf8)`
avec liaison C. Le chargeur vérifie l’export dans le PE, prépare/valide tous les
paramètres avant de charger la première DLL, appelle Configure puis Initialize.
Le texte transmis est trié, sous forme de lignes `clé=valeur`, défauts inclus.
Retour 1 attendu ; copier les pointeurs pour les conserver après cet appel.
La structure de contexte ABI 1 reste inchangée pour les mods existants.

Une configuration différente de celle d’un mod résident impose une relance.
Rétablir exactement les valeurs chargées supprime ce besoin. Pas d’application
à chaud. Test_A et Test_B ne font qu’écrire leurs valeurs et le GUID dans le
journal du chargeur : les options « effets », « couleur » et « mode » servent
à tester la configuration, sans effet sur les sauvegardes ou le gameplay.

Tests automatisés : séparation de deux mondes et deux DLL, défauts, oui/non,
choix autorisés, valeurs invalides, conflits d’écriture, conservation quand
le mod est désactivé, sauvegarde immédiate, relance et annulation du besoin de
relance, deux processus frais recevant des valeurs distinctes, configuration
corrompue refusée avant toute exécution et schéma avec doublon refusé.
Les contrôles natifs eux-mêmes restent à vérifier visuellement dans le jeu.

Pour transférer un monde avec ses réglages, copier le profil `.mods` ET le
dossier `<GUID>.settings`. Le paquet de transfert d’activation existant reste
limité au `.mods` ; il ne doit pas être présenté comme une exportation de ces
nouveaux paramètres.

Depuis 0.6.2, `percent` déclare défaut, minimum, maximum et pas, sous forme
d’entiers de 0 à 10000. Le pas doit diviser la plage, le défaut doit être sur
une graduation, et la plage doit avoir 2 à 32 graduations. Le jeu reçoit un
ratio pour son curseur natif ; le fichier conserve le pourcentage entier.
La valeur est enregistrée au relâchement. Une nouvelle clé absente d’un ancien
fichier prend sa valeur par défaut sans modifier les autres réglages.
Correction 0.6.2 : le drapeau natif `c3a` autorise les flèches des sélecteurs.

## Stockage depuis 0.6.9

Le chemin actuel est `<GUID>.settings-v2/<SHA256(id UTF-8 normalis�)>.ini`.
Le format du contenu reste ENSHROUDED_SETTINGS_V1. Le hachage utilise 64 caract�res
hexad�cimaux minuscules. La migration automatique d�place les anciens `.ini` sous
verrous, sans �craser un fichier d�j� pr�sent. Une erreur de migration est affich�e
et laisse les r�glages en place pour un nouvel essai. Un ancien fichier corrompu
reste invalide apr�s migration : il n�est pas remplac� silencieusement par des d�fauts.
Copier le dossier worlds complet pour une sauvegarde ou un transfert manuel.
