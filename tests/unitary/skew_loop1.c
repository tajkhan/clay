#pragma scop
/* Clay
  skew([0,0], 1, 2);
*/
for(i = 0 ; i <= N ; i++) {
  for(j = 0 ; j <= M ; j++) {
    a[i][j] = 0;
  }
  b[i] = 0;
}
#pragma endscop
