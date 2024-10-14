# alloc_check

Alloc_check is the rewrite of a tool I made in my first year of college to aid me in the development of a project.
We had to create a small program to manage bus networks and had to be written in C. Naturally it involved dynamic memory, and with it, memory leaks.
When I started said project I had no access to internet and valgrind was not installed, so I decided to write a helper library to check for missing frees.
The code for this was terrible so I recently decided to rewrite it. It's still bad, just has more features and is slightly less abominable.

Some very questionable decisions were made during development and they are easiliy noticeable when running larger programs with many malloc/free/realloc calls. Performance is not great, and reporting takes multiple seconds with long running programs.

This was an interesting project to make, but it might need a round 3 to see proper code quality.
