# Show Player Population

This is a BakkesMod Plugin that displays (and logs) current population values in
Rocket League, from the total players online to how many players are in a
certain game mode. That information is even presented on a graph, so you can see
how the numbers change over time.

Here's how it should look in game:
![image would go here](image file)

# To install for your BakkesMod/Rocket League 
See the [Release](https://github.com/mgavin/ShowPlayerPopulation/releases) section.

# Future plans?

Version 3.0 (or whatever major version number one higher than whatever I decide
at the time to work on this feature) is planned to have the individual
population numbers show above the playlist "cards" (icons, squares, tiles... the
boxes with the pictures that say the game mode that have the yellow checkmark
when selected). I believe it qualifies as a major revision given the nature of
the change: being one that changes the user experience and including the amount
of work it would take to implement... because I basically have to determine at
what positions on the screen the little windows should go... for every interface
scale + UI scale + resolution (because AFAIK that information isn't
accessible... it might be, it might not be, I'm not sure.)

OTHERWISE... I don't necessarily have much more planned to specifically add to this.  

Quick brainstorm of ideas:

- Turn off different sections / playlist groupings of the overlay
- Some sort of different "overlay" option
- Rotation backup of logs
    - i.e. if you want to keep your graph in game only with the last certain
      amount of hours, or just want the logged data to be split up and want it
      to be done for you instead of by hand

...

# A little how it works section can go here, but prolly later.

If you have a suggestion, you're welcome to [present it as a github issue](https://github.com/mgavin/ShowPlayerPopulation/issues)!

---

# If you have an issue...
[Raise a github issue on this repo.](https://github.com/mgavin/ShowPlayerPopulation/issues)

# If you want to fork, clone, patch, create a pull request...
1. Fork the repo
2. Clone the repo
3. CD into the repo
4. Run `git submodule init` to initialize the submodules
5. Run `git submodule update` to checkout the used revisions for the submodules
6. Run `npm install` to install git-clang-format and husky.
   These are used to establish pre-commit hooks to reformat the code in a way that's preferred for the repository.
7. Code
