// Package delim contains basic utilities for length-delimited protofiles.
package delim

import (
	"bufio"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"os"

	"google.golang.org/protobuf/proto"
)

type Reader struct {
	r io.Reader
	c io.Closer
}

func OpenReader(path string) (*Reader, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	return NewReader(bufio.NewReader(f), f), nil
}

func NewReader(r io.Reader, c io.Closer) *Reader {
	return &Reader{r: r, c: c}
}

func (r *Reader) Read(dst proto.Message, buf []byte) ([]byte, error) {
	buf = pad(buf, 4)
	if _, err := io.ReadFull(r.r, buf); err == io.EOF {
		return buf, io.EOF
	} else if err != nil {
		return buf, fmt.Errorf("read header: %w", err)
	}
	size := int(binary.LittleEndian.Uint32(buf))
	// TODO: size limits?
	buf = pad(buf, 4+size)
	if _, err := io.ReadFull(r.r, buf[4:]); err == io.EOF {
		return buf, io.EOF
	} else if err != nil {
		return buf, fmt.Errorf("read body: %w", err)
	}
	return buf, proto.Unmarshal(buf[4:], dst)
}

func (r *Reader) Close() error {
	if r.c != nil {
		return r.c.Close()
	}
	return nil
}

type Writer struct {
	w io.Writer
	c io.Closer
}

func OpenWriter(path string) (*Writer, error) {
	f, err := os.Create(path)
	if err != nil {
		return nil, err
	}
	return NewWriter(bufio.NewWriter(f), f), nil
}

func NewWriter(w io.Writer, c io.Closer) *Writer {
	return &Writer{w: w, c: c}
}

func (w *Writer) Write(src proto.Message, buf []byte) ([]byte, error) {
	mo := proto.MarshalOptions{}
	size := mo.Size(src)
	mo.UseCachedSize = true
	buf = pad(buf, 4+size)[:0]
	buf = binary.LittleEndian.AppendUint32(buf, uint32(size))
	buf, err := mo.MarshalAppend(buf, src)
	if err != nil {
		return buf, err
	}
	_, err = w.w.Write(buf)
	return buf, err
}

func (w *Writer) Close() error {
	var errF, errC error
	if f, ok := w.w.(interface{ Flush() error }); ok {
		errF = f.Flush()
	}
	if w.c != nil {
		errC = w.c.Close()
	}
	return errors.Join(errF, errC)
}

type LogWriter struct {
	path          string
	size, maxSize int
	events        chan<- proto.Message
	done          <-chan error
	w             Writer
	buf           []byte
}

func NewLogWriter(path string, maxSize, chanSize int) (*LogWriter, error) {
	f, err := os.OpenFile(path, os.O_WRONLY|os.O_CREATE, 0o666)
	if err != nil {
		return nil, err
	}
	size, err := f.Seek(0, io.SeekEnd)
	if err != nil {
		return nil, fmt.Errorf("seek to end: %w", err)
	}

	events, done := make(chan proto.Message, chanSize), make(chan error, 1)
	lw := &LogWriter{
		path: path,
		size: int(size), maxSize: maxSize,
		events: events, done: done,
		w: Writer{w: f},
	}
	go lw.run(events, done)

	return lw, nil
}

func (lw *LogWriter) Write(entry proto.Message) {
	lw.events <- entry
}

func (lw *LogWriter) Close() error {
	close(lw.events)
	return <-lw.done
}

func (lw *LogWriter) run(events <-chan proto.Message, done chan<- error) {
	var err error
	for entry := range events {
		lw.buf, err = lw.w.Write(entry, lw.buf)
		if err != nil {
			break
		}
		lw.size += len(lw.buf)
		if lw.size >= lw.maxSize {
			// TODO: make a new chunk
			err = fmt.Errorf("unimplemented: log chunking")
			break
		}
	}
	// TODO: log if breaking due to an error
	for range events {
		// drain the channel to avoid blocking writing processes
	}
	done <- err
}

func pad(buf []byte, size int) []byte {
	if cap(buf) < size {
		buf = append(buf, make([]byte, size-len(buf))...)
	}
	return buf[:size]
}
