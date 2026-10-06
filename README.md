# Pulsar

Multi-effet VST3 pour FL Studio, pensé pour retourner tes propres mélodies avant de les resampler.
Un pad X/Y central (le portail) pilote autant d'effets que tu veux d'un seul geste.

## Obtenir le plugin sans rien installer

1. Onglet **Actions** de ce dépôt, puis le dernier lancement de « Compiler Pulsar (VST3 Windows) » (coche verte).
2. En bas de la page, section **Artifacts** : télécharge `Pulsar-VST3-Windows`.
3. Dézippe. Tu obtiens un dossier `Pulsar.vst3`.
4. Copie ce dossier dans `C:\Program Files\Common Files\VST3\`.
5. Dans FL Studio : **Options** > **Manage plugins** > **Find installed plugins**. Pulsar apparaît dans les effets.

Si l'onglet Actions ne lance rien, c'est que le dossier `.github` n'est pas monté (il est masqué).
Crée alors le fichier à la main : **Add file** > **Create new file**, nom `.github/workflows/build.yml`,
et colle le contenu du fichier `build.yml` fourni.

## Comment s'en servir

Pulsar est un effet : pose-le sur la piste de ta mélodie dans la table de mixage.

- **Univers** (en haut au centre) : Nuage, Lo-fi, Rythme, Cosmos, Finition.
- **Style** (en haut à droite) : une chaîne complète, avec ce que le portail pilote. Tout reste modifiable.
- **Le portail** : déplace le point. En bas à gauche, les potards restent où ils sont posés ;
  plus tu t'éloignes, plus X et Y les emmènent loin. Le panneau « Style » dit ce que fait chaque axe.
- **Assigner X / Assigner Y** : active le bouton, puis tourne les potards que cet axe doit piloter.
  Tu règles leur course (de -100 % à +100 %). Double-clic sur un potard pour le retirer. Désactive le bouton ensuite.
- **Orbite** : le point bouge tout seul, calé sur le tempo. Il tourne autour de l'endroit où tu l'as posé.
- Sur chaque potard : l'arc doré est la valeur posée, le petit satellite est la valeur réelle (il bouge avec le portail),
  l'arc pâle montre la course donnée par X, l'arc ambré celle donnée par Y.
- Survole un potard pour lire sa valeur et une explication.

Pour enregistrer un mouvement : clic droit sur « Portail X » ou « Portail Y » dans la liste des paramètres de FL Studio
et crée une automation, ou laisse faire l'Orbite. Ensuite, rends la piste en audio : c'est ton resample.

## Les effets, dans l'ordre

| Module | Ce qu'il fait |
|---|---|
| Temps | Demi-vitesse, inversé, répétition, calés sur le tempo. Pompe = volume qui plonge à chaque temps. |
| Grain | Découpe le son en grains : pitch sans changer la durée, dispersion, grains inversés, éclats à l'octave. |
| Usure | Wobble de bande, pertes de signal, bruit (vinyle, bande, secteur). |
| Chaleur | Saturation (lampe, bande, clip, radio) et crush numérique. |
| Filtre | Passe-bas, passe-bande ou passe-haut, 24 dB par octave. |
| Modulation | Chorus, flanger ou phaser. |
| Écho | Ping-pong calé sur le tempo. |
| Espace | Réverbe de 0,4 s à 18 s. |
| Finition | Colle (compression sur trois bandes), poids, air, largeur stéréo. |
| Sortie | Mix, volume, limiteur de sécurité. |

## Ajouter ou modifier un style

Tout est dans `Source/Presets.h`. Copie un bloc, change le nom et les valeurs :

```cpp
{ "Mon style", "Une phrase qui le décrit.", "ce que fait X", "ce que fait Y",
  { { P::grainMix, 70 },                    // potard posé à 70 %
    { P::filterCutoff, 9000, -40, 0 },      // posé à 9 kHz, X le fait descendre de 40 % de sa course
    { P::revMix, 20, 0, 50 },               // posé à 20 %, Y le fait monter de 50 %
    { P::padX, 30 }, { P::padY, 30 } } },   // où le point est posé au chargement
```

Enregistre le fichier sur GitHub : la compilation repart toute seule.

## Compiler sur ton PC (facultatif)

Avec Visual Studio Community (charge de travail « Développement Desktop en C++ ») et CMake :

```
cmake -B build
cmake --build build --config Release --target Pulsar_VST3
```

Le plugin se trouve ensuite dans `build/Pulsar_artefacts/Release/VST3/`.

## Structure du code

```
Source/dsp/Table.h      la liste de tous les réglages (bornes, valeurs par défaut)
Source/dsp/Engine.h     le moteur : portail, orbite, chaîne d'effets
Source/dsp/Time.h       orbite, effets de temps, pompe
Source/dsp/Grain.h      le nuage de grains
Source/dsp/Wear.h       wobble, pertes, bruit
Source/dsp/Color.h      saturation, crush, filtre, modulation
Source/dsp/Space.h      écho et réverbe
Source/dsp/Finish.h     colle, ton, largeur, limiteur
Source/Presets.h        les univers et leurs styles
Source/Look.h           couleurs, potards
Source/PluginEditor.*   l'interface et le portail
Source/PluginProcessor.* le lien avec FL Studio
tests/                  banc de test du moteur et outil de capture d'écran
```

## Limites connues

- Latence annoncée à l'hôte : 31 échantillons (moins d'une milliseconde).
- Le Wobble retarde très légèrement le son traité (quelques millisecondes), comme une vraie bande.
- Les effets de temps ont besoin du tempo du projet : ils se calent sur la grille quand le morceau joue.
