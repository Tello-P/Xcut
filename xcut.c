#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>



// Global variables to store the clipboard text and its length.
char *clipboard_text = NULL;
size_t text_length = 0;


/*
 * Function: my_strdup
 * -------------------
 * Creates a duplicate of the given string by allocating memory
 * and copying the content of the original string.
 *
 * Parameters:
 *   s - Pointer to the null-terminated string to duplicate.
 *
 * Returns:
 *   A pointer to the newly allocated duplicate of the string,
 *   or NULL if memory allocation fails.
 *
 * Example:
 *   char *original = "Hello, World!";
 *   char *copy = my_strdup(original);
 *   if (copy != NULL) {
 *       // Use the duplicated string...
 *       free(copy); // Don't forget to free the allocated memory.
 *   }
 */
char *my_strdup(const char *s)
{
    size_t len = strlen(s) + 1;  // +1 for the terminating null byte.
    char *copy = malloc(len);
    if (copy != NULL)
    {
        memcpy(copy, s, len);
    }
    return copy;
}

/*
 * Function: handle_selection_request
 * ------------------------------------
 * Handles selection requests from other applications.
 * When another application requests the clipboard content,
 * this function sends the text stored in 'clipboard_text'.
 *
 * Parameters:
 *   display - connection to the X server.
 *   event   - pointer to the X event that contains the selection request.
 *
 * Example:
 *   Suppose an application issues a paste command. It sends a 
 *   SelectionRequest event asking for the content in either XA_STRING
 *   or UTF8_STRING format. This function responds with the stored text.
 */
void handle_selection_request(Display *display, XEvent *event) {
    // Cast the generic event to a selection request event.
    XSelectionRequestEvent *req = &event->xselectionrequest;
    XEvent response;
    // Zero out the response structure to avoid garbage values.
    memset(&response, 0, sizeof(response));

    // Set up the response event fields.
    response.xselection.type      = SelectionNotify;  // Notify event type.
    response.xselection.display   = req->display;       // Use the same display.
    response.xselection.requestor = req->requestor;     // The window requesting the selection.
    response.xselection.selection = req->selection;     // The clipboard selection.
    response.xselection.target    = req->target;        // Requested data format.
    response.xselection.time      = req->time;          // Timestamp from the request.
    // By default, we use the property specified by the requestor.
    response.xselection.property  = req->property;

    // Define an atom for UTF8_STRING.
    Atom utf8_string = XInternAtom(display, "UTF8_STRING", False);

    // Check the requested target format.
    if (req->target == XA_STRING || req->target == utf8_string) {
        // If the target is a string format (ASCII or UTF-8),
        // transfer the clipboard text to the requested property.
        XChangeProperty(display, req->requestor, req->property,
                        req->target, 8, PropModeReplace,
                        (unsigned char*)clipboard_text, text_length);
    } else if (req->target == XInternAtom(display, "TARGETS", False)) {
        // If the request is for the list of supported formats,
        // provide an array containing XA_STRING, UTF8_STRING, and TARGETS.
        Atom targets[3];
        int count = 0;
        targets[count++] = XA_STRING;
        targets[count++] = utf8_string;
        targets[count++] = XInternAtom(display, "TARGETS", False);
        XChangeProperty(display, req->requestor, req->property,
                        XA_ATOM, 32, PropModeReplace,
                        (unsigned char*)targets, count);
    } else {
        // If the requested target is not supported, set property to None.
        response.xselection.property = None;
    }

    // Send the response event to the requestor.
    XSendEvent(display, req->requestor, 0, 0, &response);
    XFlush(display);
}

int main(void) {
    // 1. Prompt the user to enter the text to copy to the clipboard.
    printf("Enter the text to copy to the clipboard:\n");

    /*
     * Instead of using getline (which is not available in all compilers),
     * we use fgets with a fixed-size buffer.
     *
     * Example:
     *   If the user types "Hello, World!\n", fgets will capture that line 
     *   (up to 1023 characters) and store it in the buffer.
     */
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        perror("Error reading text");
        exit(EXIT_FAILURE);
    }
    // Duplicate the buffer to allocate just enough memory for the clipboard text.
    clipboard_text = my_strdup(buffer);
    if (clipboard_text == NULL) {
        perror("Error allocating memory for clipboard text");
        exit(EXIT_FAILURE);
    }
    // Determine the length of the text (including the newline, if present).
    text_length = strlen(clipboard_text);

    // 2. Connect to the X server.
    Display *display = XOpenDisplay(NULL); // NULL uses the DISPLAY environment variable.
    if (display == NULL) {
        fprintf(stderr, "Unable to open display.\n");
        free(clipboard_text);
        exit(EXIT_FAILURE);
    }
    int screen = DefaultScreen(display); // Get the default screen number.

    // 3. Create an invisible window to act as the clipboard owner.
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
                                        0, 0, 1, 1, 0,
                                        BlackPixel(display, screen),
                                        WhitePixel(display, screen));
    // Select events that we are interested in (e.g., property change events).
    XSelectInput(display, window, PropertyChangeMask);

    // 4. Set the window as the owner of the CLIPBOARD selection.
    Atom clipboard_atom = XInternAtom(display, "CLIPBOARD", False);
    XSetSelectionOwner(display, clipboard_atom, window, CurrentTime);
    if (XGetSelectionOwner(display, clipboard_atom) != window) {
        fprintf(stderr, "Failed to acquire clipboard ownership.\n");
        free(clipboard_text);
        XCloseDisplay(display);
        exit(EXIT_FAILURE);
    }

    printf("Text has been copied to the clipboard.\n");
    printf("The program will remain running to maintain the clipboard content.\n");
    printf("Press Ctrl+C to exit.\n");

    // 5. Event loop to handle selection requests.
    XEvent event;
    while (1) {
        XNextEvent(display, &event);
        switch (event.type) {
            case SelectionRequest:
                // When a SelectionRequest event is received, handle it.
                handle_selection_request(display, &event);
                break;
            case SelectionClear:
                // When the clipboard ownership is lost, notify the user.
                printf("Clipboard ownership lost.\n");
                goto cleanup;
            default:
                // Other events are ignored.
                break;
        }
    }

cleanup:
    // 6. Clean up resources before exiting.
    free(clipboard_text);            // Free the allocated clipboard text.
    XDestroyWindow(display, window); // Destroy the window.
    XCloseDisplay(display);          // Close the connection to the X server.
    return 0;
}

