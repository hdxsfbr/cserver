#ifndef CONTACTS_H
#define CONTACTS_H

typedef struct {
    int id;              // Unique identifier for the contact
    char name[100];      // Name of the contact
    char email[100];     // Email of the contact
    char phone[15];      // Phone number of the contact
} Contact;

// Contact storage helpers
int contacts_count(void);
const Contact *contacts_all(void);
int contacts_get(int id, Contact *out_contact);
int contacts_create(const Contact *input, Contact *created_contact);
int contacts_update(int id, const Contact *input, Contact *updated_contact);
int contacts_delete(int id);

#endif // CONTACTS_H
