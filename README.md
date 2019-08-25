# NSxfer ([NSIS](https://nsis.sourceforge.io/Main_Page) plugin)
NSxfer gives you the means to perform complex HTTP/HTTPS transfers from a NSIS script<br>
The plugin is included in my **unofficial** [NSIS builds](https://github.com/negrutiu/nsis)

### Features:
- **Multi threaded**: transfer multiple files in parallel
- **Asynchronous**: start a download now, check its status later
- **Aggressive**: multiple attempts to connect, reconnect, resume interrupted transfers, etc.
- **NSIS aware**: download files at any installation stage (from `.onInit` callback, from `Sections`, from custom pages, silent installers, etc.)
- **Informative**: plenty of useful information is available for each transfer (size, speed, HTTP status, HTTP headers, etc)
- Supports all relevant **HTTP verbs** (GET, POST, PUT, HEAD, etc)
- Supports **custom HTTP headers and data**
- Supports **proxy servers** (both authenticated and open)
- Supports files larger than **4GB**
- Can download remote content to **RAM** instead of a file
- Works well in **64-bit** [NSIS builds](https://github.com/negrutiu/nsis)
- Many more... Check out the included [readme file](NSxfer.Readme.txt)

### Basic usage:
- HTTP GET example:
```
Delete "$TEMP\MyFile.json"	; If the file exists, the transfer will resume
NSxfer::Transfer /NOUNLOAD /URL "https://httpbin.org/get?param1=1&param2=2" /LOCAL "$TEMP\Response.json" /END
Pop $0 				; Status text ("OK" for success)
```
- HTTP POST `application/json`:
```
Delete "$TEMP\MyFile.json"	; If the file exists, the transfer will resume
NSxfer::Transfer /NOUNLOAD /URL "https://httpbin.org/post?param1=1&param2=2" /LOCAL "$TEMP\MyFile.json" /METHOD POST /DATA '{"number_of_the_beast" : 666}' /HEADERS "Content-Type: application/json" /END
Pop $0 				; Status text ("OK" for success)
```
- HTTP POST `application/x-www-form-urlencoded`:
```
Delete "$TEMP\MyFile.json"	; If the file exists, the transfer will resume
NSxfer::Transfer /NOUNLOAD /URL "https://httpbin.org/post?param1=1&param2=2" /LOCAL "$TEMP\MyFile.json" /METHOD POST /DATA 'User=My+User&Pass=My+Pass' /HEADERS "Content-Type: application/x-www-form-urlencoded" /END
Pop $0 				; Status text ("OK" for success)
```
- More complex examples in the [readme file](NSxfer.Readme.txt)
