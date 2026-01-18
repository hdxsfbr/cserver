#include <stdio.h>
#include <string.h>
#include "contacts.h"

// Array to hold contacts
Contact contacts[100];
int contact_count = 0;

void create_contact(Contact contact) {
    if (contact_count < 100) {
        contacts[contact_count] = contact;  // Add contact to the array
        contact_count++;  // Increment the count of contacts
    } else {
        fprintf(stderr, "Error: Contact list is full.\n");
    }
}

void read_contacts() {
    for (int i = 0; i < contact_count; i++) {
        printf("ID: %d, Name: %s, Email: %s, Phone: %s\n", 
               contacts[i].id, contacts[i].name, contacts[i].email, contacts[i].phone);
    }
}

void update_contact(int id, Contact updated_contact) {
    for (int i = 0; i < contact_count; i++) {
        if (contacts[i].id == id) {
            contacts[i] = updated_contact;  // Update the contact
            return;
        }
    }
    fprintf(stderr, "Error: Contact with ID %d not found.\n", id);
}

void delete_contact(int id) {
    for (int i = 0; i < contact_count; i++) {
        if (contacts[i].id == id) {
            // Shift contacts down to remove the contact
            for (int j = i; j < contact_count - 1; j++) {
                contacts[j] = contacts[j + 1];
            }
            contact_count--;  // Decrement the contact count
            return;
        }
    }
    fprintf(stderr, "Error: Contact with ID %d not found.\n", id);
}