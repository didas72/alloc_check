# alloc_check

Alloc_check is the rewrite of a tool I made in my first year of college to aid me in the development of a project.
We had to create a small program to manage bus networks and had to be written in C. Naturally it involved dynamic memory, and with it, memory leaks.
When I started this project I had no access to internet and did not have valgrind installed, so I decided to write a helper library to check for missing frees.
The code for this was terrible so I recently decided to rewrite it. It's still bad, just has more features and is slightly less terrible.

Some questionable decisions were made and they are easiliy noticeable with larger programs that frequently call alloc/free/realloc and functions alike. Performance is not great, and reporting takes multiple seconds with long running programs.

Was an interesting project to make, but it might need a round 3 to see proper code quality.
