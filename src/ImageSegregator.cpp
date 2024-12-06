#include <gtk/gtk.h>
#include <vector>
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp> // apt install nlohmann-json3-dev

namespace fs = std::filesystem;
using json = nlohmann::json;

std::vector<std::string> image_paths;
size_t current_image_index = 0;
GtkWidget *image_widget;
GtkWidget *capslock_label;
std::string folder_path;

// Structure for button data from JSON
struct ButtonData {
    std::string label;
    std::string target_path;
    std::string key;
};
std::vector<ButtonData> button_data;



bool is_image(const fs::path& path) {
    std::string ext = path.extension().string();
    return ext == ".JPG" || ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp";
}

void load_images(const std::string& folder_path) {
    image_paths.clear();
    for (const auto& entry : fs::directory_iterator(folder_path)) {
        if (entry.is_regular_file() && is_image(entry.path())) {
            image_paths.push_back(entry.path().string());
        }
    }
}

void load_button_data(const std::string& json_path) {
    button_data.clear();
    std::ifstream file(json_path);
    if (!file) {
        GtkWidget *dialog = gtk_message_dialog_new(NULL,
                                                   GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_OK,
                                                   "Error: Unable to open JSON file: %s",
                                                   json_path.c_str());
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    json j;
    file >> j;

    for (const auto& entry : j) {
        ButtonData data;
        data.label = entry["value"];
        data.target_path = entry["path"];
        if (entry.contains("key")) {
            data.key = entry["key"];
        }
        button_data.push_back(data);
    }
}

void display_image_with_max_height(const std::string& image_path, int max_height) {
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(image_path.c_str(), NULL);
    if (pixbuf) {
        int width = gdk_pixbuf_get_width(pixbuf);
        int height = gdk_pixbuf_get_height(pixbuf);
        if (height > max_height) {
            int new_width = width * max_height / height;
            GdkPixbuf *scaled_pixbuf = gdk_pixbuf_scale_simple(pixbuf, new_width, max_height, GDK_INTERP_BILINEAR);
            gtk_image_set_from_pixbuf(GTK_IMAGE(image_widget), scaled_pixbuf);
            g_object_unref(scaled_pixbuf);
        } else {
            gtk_image_set_from_pixbuf(GTK_IMAGE(image_widget), pixbuf);
        }
        g_object_unref(pixbuf);
    }
}

void on_button_move_clicked(GtkWidget *widget, gpointer data) {
    if (image_paths.empty()) return;

    ButtonData *button_data = static_cast<ButtonData*>(data);

    fs::path target_folder = button_data->target_path;
    if (!fs::exists(target_folder)) {
        fs::create_directories(target_folder);
    }

    std::string current_image_path = image_paths[current_image_index];
    fs::path source_path(current_image_path);
    fs::path destination_path = target_folder / source_path.filename();

    try {
        fs::rename(source_path, destination_path);
        //std::cout << "Image moved to: " << destination_path << std::endl;

        image_paths.erase(image_paths.begin() + current_image_index);
        if (current_image_index >= image_paths.size()) {
            current_image_index = 0;
        }
        if (!image_paths.empty()) {
            display_image_with_max_height(image_paths[current_image_index], 600);
        } else {
            gtk_image_clear(GTK_IMAGE(image_widget));
        }
    } catch (const fs::filesystem_error& e) {
        //std::cerr << "Error moving file: " << e.what() << std::endl;
        GtkWidget *dialog = gtk_message_dialog_new(NULL,
                                                   GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_OK,
                                                   "Error: mofinv file: %s",
                                                   e.what());
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
}

gboolean on_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    for (auto& data : button_data) {
        if (data.key.empty()) continue;
        if (event->keyval == gdk_unicode_to_keyval(data.key[0])) {
            GtkWidget *button = gtk_button_new_with_label(data.label.c_str());
            g_signal_connect(button, "clicked", G_CALLBACK(on_button_move_clicked), &data);
            on_button_move_clicked(button, &data);
            return TRUE;
        }
    }
    return FALSE;
}

GtkWidget *dynamic_button_container;

void on_open_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Select Folder",
                                                    GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                                                    GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                    "Cancel", GTK_RESPONSE_CANCEL,
                                                    "Select", GTK_RESPONSE_ACCEPT,
                                                    NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        folder_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        load_images(folder_path);

        std::string json_path = folder_path + "/imageSegregator.json";
        load_button_data(json_path);

        // Usuń poprzednie dynamiczne przyciski
        GList *children = gtk_container_get_children(GTK_CONTAINER(dynamic_button_container));
        for (GList *iter = children; iter != NULL; iter = iter->next) {
            gtk_widget_destroy(GTK_WIDGET(iter->data));
        }
        g_list_free(children);

        // Dodaj nowe przyciski na podstawie danych z JSON
        for (auto& data : button_data) {
            GtkWidget *button = gtk_button_new_with_label(data.label.c_str());
            g_signal_connect(button, "clicked", G_CALLBACK(on_button_move_clicked), &data);
            gtk_box_pack_start(GTK_BOX(dynamic_button_container), button, FALSE, FALSE, 0);
        }

        gtk_widget_show_all(dynamic_button_container);

        if (!image_paths.empty()) {
            current_image_index = 0;
            display_image_with_max_height(image_paths[current_image_index], 600);
        }
    }

    gtk_widget_destroy(dialog);
}


gboolean update_capslock_label(gpointer data) {
    GdkDisplay *display = gdk_display_get_default();
    GdkKeymap *keymap = gdk_keymap_get_for_display(display);

    if (keymap) {
        guint modifier_state = gdk_keymap_get_modifier_state(keymap);
        const gchar *status = (static_cast<GdkModifierType>(modifier_state) & GDK_LOCK_MASK) ? "Caps Lock is ON" : "Caps Lock is OFF";
        gtk_label_set_text(GTK_LABEL(capslock_label), status);
    }
    return TRUE;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "ImageSegregator");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    GtkWidget *image_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    image_widget = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(image_container), image_widget, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), image_container, TRUE, TRUE, 0);

    GtkWidget *button_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    // Kontener na dynamiczne przyciski
    dynamic_button_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(button_container), dynamic_button_container, FALSE, FALSE, 0);

    capslock_label = gtk_label_new("Caps Lock state");
    gtk_box_pack_start(GTK_BOX(button_container), capslock_label, FALSE, FALSE, 0);

    GtkWidget *button_open = gtk_button_new_with_label("Open Folder");
    g_signal_connect(button_open, "clicked", G_CALLBACK(on_open_button_clicked), button_container);
    gtk_box_pack_start(GTK_BOX(button_container), button_open, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(hbox), button_container, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), hbox);

    g_timeout_add(500, update_capslock_label, NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press_event), NULL);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}


