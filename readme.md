# png chunks doc

https://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html

# png file structure doc

https://www.libpng.org/pub/png/spec/1.2/PNG-Structure.html

# adam7 in my own words

the image data consists of 7 consecutive images that when properly overlapped create the final image.
in order to properly parse adam7 data, one needs to properly parse each image, i.e. apply the proper filters per
scanline.
the dimensions of each image plus the final position of each pixel is calculated by considering the following 8x8
pattern that is repeated accross the entire image.
the number indicates in which pass the according pixel is "transmitted".
if we assume a final image of 16x16 pixels, the 3rd pass would consist of 2 scanlines of 4 pixels.<br>
1 6 4 6 2 6 4 6<br>
7 7 7 7 7 7 7 7<br>
5 6 5 6 5 6 5 6<br>
7 7 7 7 7 7 7 7<br>
3 6 4 6 3 6 4 6<br>
7 7 7 7 7 7 7 7<br>
5 6 5 6 5 6 5 6<br>
7 7 7 7 7 7 7 7<br>

## pass 1

offset = 0<br>
one top left pixel per 8x8 pattern<br>
row length = image_width/8 * bpp + 1<br>
rows = image_height/8 <br>

final_x = 0 + 64 * x <br>
final_y = 0 + row_len * 64 * y

# pass 2

offset = pass1_row_length * pass1_rows<br>
one pixel in the 5th position (index 4) in the first row <br>
row length = image_width/8 * bpp + 1<br>
rows = image_height/8

final_buff_pos =

# pass 3

two pixels at 0 and 4 of row 4<br>
row length = 2 * image_width/8 * bpp + 1<br>
rows = image_height/8

# pass 4

two pixels at 2 and 6<br>
of rows 0 and 4<br>
row length = 2 * image_width/8 * bpp + 1<br>
rows = 2 * image_height/8

# pass 5

4 pixels at 0, 2, 4, 6
of rows 2 and 6
row length = 4 * image_width/8 * bpp +1 <br>
rows = 2 * image_height/8

# pass 6

now it gets interesting<br>
pixels at 1, 3, 5, 7 <br>
of rows 0, 2, 4, 6 <br>
row length = 4 * image_width/8 * bpp + 1 <br>
rows = 4 * image_height/8

# pass 7

all of row 1, 3, 5, 7
row_length = image_width * bpp + 1 <br>
rows = 4 * image_height/8

# formula for real pos

final_arr_pos = start_pos + 8 * x / width_factor * y // aber nur für pass 0!! andere sind komplexer!

müsste stimmen, kann sicher vereinfacht werden, da width/height und die starts und steps zusammenhängen
finalfinal_pos = start_pos_x + step_x * x / width_factor * (start_pos_y + step_y * y / height_factor)
