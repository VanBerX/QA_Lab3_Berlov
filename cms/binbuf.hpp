#ifndef BINBUF_HPP_SENTRY
#define BINBUF_HPP_SENTRY


/*
   This module as a whole is borrowed from the SUE library; the only
   change is that the class, originaly named SUEBuffer, is renamed to
   BinaryBuffer.

   Fortunately, this module doesn't depend on anything except some
   functions from string.h, such as memcpy, memmov and memset.
 */

//! Buffer used by sessions
class BinaryBuffer {
    char *data;
    int datalen;
    int maxlen;
public:
      //! Constructor
    BinaryBuffer();
      //! Destructor
    ~BinaryBuffer();

      //! Add some data to the end
      /*! The buffer is enlarged and data is added */
    void AddData(const char *buf, int size);

      //! Move data from the buffer
      /*! The first bufsize bytes from the buffer are
          copied to the user-supplied memory pointed by buf.
          If the object contains less data than bufsize, then
          all the present data is copied. The method returns
          count of the copied bytes. The copied data is removed
          from the buffer.
       */
    int GetData(char *buf, int bufsize);

      //! Drop some data
      /*! The first len bytes in the buffer are removed. If the
          buffer contains less than len bytes, it is just emptied.
       */
    void DropData(int len);

      //! Erase given range from the buffer
    void EraseData(int index, int len);

      //! Empty the buffer
    void DropAll() { datalen = 0; }

      //! Add a char to the end of the buffer
    void AddChar(char c);

      //! Add a string to the end of the buffer
      /*! The string pointed by str is added to the buffer.
          Terminating zero is not copied, only the string itself.
       */
    void AddString(const char *str);

      //! Read a line
      /*! Checks whether a text line (that is, something terminated
          by the EOL character) is available at the beginning of the
          buffer. If so, copy it to the user-supplied memory and remove
          it from the buffer.
          \note The zero byte is always stored into the caller's buffer,
          so at most bufsize-1 bytes are copied. If there is no room
          for the whole string, then only a part is copied.
          The terminating EOL is never stored in the buffer.
          \note In case of CRLF, the '\r' symbol is striped off
       */
    int ReadLine(char *buf, int bufsize);

      //! Read a line into another buffer
      /*! Checks whether a text line (that is, something terminated
          by the EOL character) is available at the beginning of the
          buffer. If so, copy it to the user-supplied buffer and remove
          it from the buffer.
          \note The terminating EOL is never stored in the buffer.
          \note In case of CRLF, the '\r' symbol is striped off
       */
    bool ReadLine(BinaryBuffer &buf);

      //! Find the given line in the buffer
      /*! returns the index of the '\n' right after the marker */
    int FindLineMarker(const char *marker) const;
    int ReadUntilLineMarker(const char *marker, char *buf, int bufsize);
    bool ReadUntilLineMarker(const char *marker, BinaryBuffer &dest);

      //! Does the buffer contain exactly given text
      /*! Checks if the buffer's content is exactly the same
          as in the given zero-terminated string.
       */
    bool ContainsExactText(const char *str) const;

      //! Return the pointer to the actual data
      /*! \note only the first len bytes makes sence,
          where len is what Length() method returns.
          Accessing addresses beyond this amount could
          lead to an unpredictable behaviour and crash
          the program!
       */
    const char *GetBuffer() const { return data; }

      //! How much data is in the buffer
      /*! How many bytes does the buffer contain */
    int Length() const { return datalen; }

      //! Access the given byte
      /*! \warning No range checking is performed. Passing
          negative i or i more than the current buffer length
          (as returned by Length() method) could lead to an
          unpredictable behaviour and/or crash.
       */
    char& operator[](int i) const { return *(data + i); }

private:
    void ProvideMaxLen(int n);
};






#endif
