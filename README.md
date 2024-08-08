Requires the TFT_eSPI library (rev. `0a47730`), which is configured in
a kind of strange way.  You need to do the following:

```
$ ln -s ${LTN92_dir}/LTN92_ST7796.h ${TFT_eSPI_dir}/User_Setups/
```

And apply:

```
diff --git a/User_Setup_Select.h b/User_Setup_Select.h
index 6e1b625..beb3e75 100644
--- a/User_Setup_Select.h
+++ b/User_Setup_Select.h
@@ -24,7 +24,7 @@
 
 // Only ONE line below should be uncommented to define your setup.  Add extra lines and files as needed.
 
-#include <User_Setup.h>           // Default setup is root library folder
+#include <User_Setups/LTN_ST7796.h>
 
 //#include <User_Setups/Setup1_ILI9341.h>  // Setup file for ESP8266 configured for my ILI9341
 //#include <User_Setups/Setup2_ST7735.h>   // Setup file for ESP8266 configured for my ST7735
```

You will also need an up-to-date version of the
[Teensy X-Plane plugin](https://github.com/jbliesener/X-Plane_Plugin.git)
(rev. `18de7d6`).
