// Package proto contains protobufs for esogem/server.
package proto

// TODO: consider pinning the protoc-gen-go binary to the version used here.

//go:generate protoc -I. --go_out=.. --go_opt=module=github.com/fis/esolangs/esogem/server config.proto model.proto log.proto
