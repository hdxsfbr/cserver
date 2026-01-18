#ifndef CONTACTS_H
#define CONTACTS_H

#include <stdio.h>

typedef struct {
    int id;              // Unique identifier for the contact
    char name[100];      // Name of the contact
    char email[100];     // Email of the contact
    char phone[15];      // Phone number of the contact
} Contact;

// Function prototypes
void create_contact(Contact contact);
void read_contacts();
void update_contact(int id, Contact updated_contact);
void delete_contact(int id);

#endif // CONTACTS_H
