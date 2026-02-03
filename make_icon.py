import struct

def create_cursor_ico(filename):
    # 32x32 pixel data (ARGB)
    width = 32
    height = 32
    
    # Simple Cursor Shape (Arrow)
    # 0 = Transparent, 1 = Black Border, 2 = White Fill
    # Rough 16x16 shape scaled
    pixels = [[0]*32 for _ in range(32)]
    
    # Draw arrow logic
    for y in range(24):
        for x in range(24):
            # Outline
            if x == 0 and y < 20: pixels[y][x] = 1
            if x == y and x < 14: pixels[y][x] = 1
            if y == 19 and x < 6: pixels[y][x] = 1
            
            # Straight bottom
            # Diagonal inner
            
    # Let's do a hardcoded 16x16 bitmap logic expanded
    # Drawing logic simpler:
    # 0,0 to 0,20 (Left edge)
    # 0,0 to 14,14 (Diagonal right edge)
    # 0,20 to 6,14 (Bottom Diagonal)
    
    # Just fill pixel buffer
    # Format: BGRA (Little Endian for Windows Bitmap)
    bitmap_data = bytearray()
    
    for y in range(height-1, -1, -1): # Bottom-up
        for x in range(width):
            # Simple Arrow Logic
            is_black = False
            is_white = False
            
            # Left vertical line
            if x == 2 and y >= 2 and y <= 26: is_black = True
            # Right diagonal
            if x == y and x >= 2 and x <= 18: is_black = True
            # Bottom segment
            if y == 26 and x >= 2 and x <= 8: is_black = True
            if x == 8 and y >= 18 and y <= 26: is_black = True # Stem side
            # Fill
            if x > 2 and x < y and x < 18 and not (x>=8 and y>=18): is_white = True
            
            if is_black:
                bitmap_data.extend([0, 0, 0, 255]) # Black opaque
            elif is_white:
                bitmap_data.extend([255, 255, 255, 255]) # White opaque
            else:
                bitmap_data.extend([0, 0, 0, 0]) # Transparent

    # BMP Header for ICO
    bmp_header_size = 40
    pixel_data_size = len(bitmap_data)
    
    # ICO Directory
    # Reserved (2), Type (2) (1=Icon), Count (2)
    ico_header = struct.pack('<HHH', 0, 1, 1)
    
    # Directory Entry
    # Width (1), Height (1), Palette (1), Reserved (1), Planes (2), BPP (2), Size (4), Offset (4)
    entry = struct.pack('<BBBBHHII', 
                        width, height, 0, 0, 
                        1, 32, 
                        bmp_header_size + pixel_data_size, 
                        22) # 6 + 16 = 22 offset
                        
    # BITMAPINFOHEADER
    # Size (4), Width (4), Height (4) (doubled for XOR mask? No, for normal BMP), Planes (2), BPP (2), Compression (4), ImageSize (4), ...
    bmp_header = struct.pack('<IIIHHIIIIII',
                             40, width, height * 2, 1, 32, 0, pixel_data_size, 0, 0, 0, 0)
                             
    with open(filename, 'wb') as f:
        f.write(ico_header)
        f.write(entry)
        f.write(bmp_header)
        f.write(bitmap_data)
        # Apply AND mask (1 bit per pixel) - all 0 for now (using Alpha channel instead)
        # 32 pixels / 8 bits = 4 bytes per row
        # 32 rows * 4 bytes = 128 bytes
        f.write(bytearray(128)) 

if __name__ == '__main__':
    create_cursor_ico("app.ico")
